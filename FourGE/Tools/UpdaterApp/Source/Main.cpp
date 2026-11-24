// =====================================================================
//  4GE UPDATER (owner 2026-11-24: "a program in the background with the 4GE look and the sonicsystems logo",
//  "the box every 10 minutes annoys me"). A tray app:
//   - every 10 minutes asks GitHub for the release "4ge-latest" (the CI build of every push to main);
//   - a new build + AUTO on + FL Studio closed -> downloads the zip, unpacks it, swaps
//     C:\Program Files\Common Files\VST3\4GE.vst3 (the new copy goes next to the old one first: a failed copy never leaves
//     FL without 4GE); FL open -> waits and swaps after FL closes;
//   - NEVER a pop-up: the state is in the tray icon's tooltip and in the window (click the tray icon);
//   - %LOCALAPPDATA%\4GE_Updater: settings.xml (installed build, AUTO) + log.txt.
//  Started at logon by a scheduled task with the highest rights (Zainstaluj_4GE_Updater.bat), --background = tray only.
// =====================================================================
#include <juce_gui_extra/juce_gui_extra.h>
#include "BinaryData.h"
#include "DoomsdayBrand.h"
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <tlhelp32.h>
#endif

namespace upd
{
    using namespace juce;
    static const char* repo = "ssonicsystemss/Doomsday";
    static const char* tag = "4ge-latest";
    static const char* assetName = "4GE-VST3-Windows.zip";

    // ---- 4GE palette (FourGE/Source/GUI/Palette.h)
    namespace col
    {
        static const Colour ground (0xff111214), panel (0xff17181b), control (0xff1d1f23), line (0xff26292e),
                            red (0xffe33135), hot (0xfff05a50), silver (0xffd5d7da), dim (0xffa2a7ae), green (0xff4caf6a), amber (0xffe6a23c);
    }
    static Font barlow (float h, bool bold)
    {
        static auto med = Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedMedium_ttf, BinaryData::BarlowSemiCondensedMedium_ttfSize);
        static auto bld = Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedBold_ttf, BinaryData::BarlowSemiCondensedBold_ttfSize);
        return Font (FontOptions (bold ? bld : med).withHeight (h));
    }

    static File dataDir() { auto d = File::getSpecialLocation (File::userApplicationDataDirectory).getParentDirectory().getChildFile ("Local").getChildFile ("4GE_Updater");
                            #if ! JUCE_WINDOWS
                            d = File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("4GE_Updater");
                            #endif
                            d.createDirectory(); return d; }
    static File vst3Target()
    {
       #if JUCE_WINDOWS
        return File ("C:\\Program Files\\Common Files\\VST3\\4GE.vst3");
       #else
        return dataDir().getChildFile ("VST3").getChildFile ("4GE.vst3");      // (Linux build = a compile check)
       #endif
    }
    static void log (const String& m)
    {
        auto f = dataDir().getChildFile ("log.txt");
        if (f.getSize() > 512 * 1024) f.deleteFile();
        f.appendText (Time::getCurrentTime().formatted ("%Y-%m-%d %H:%M:%S  ") + m + "\n");
    }
    static bool flOpen()
    {
       #if JUCE_WINDOWS
        bool found = false;
        HANDLE snap = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return false;
        PROCESSENTRY32W e; e.dwSize = sizeof (e);
        if (Process32FirstW (snap, &e))
            do { const String n (e.szExeFile); if (n.equalsIgnoreCase ("FL64.exe") || n.equalsIgnoreCase ("FL.exe")) { found = true; break; } }
            while (Process32NextW (snap, &e));
        CloseHandle (snap);
        return found;
       #else
        return false;
       #endif
    }

    struct Release { bool ok = false; String id, sha, url, date, msg, error; };

    static std::unique_ptr<InputStream> open (const String& address, int timeoutMs = 30000)
    {
        auto o = URL::InputStreamOptions (URL::ParameterHandling::inAddress)
                    .withExtraHeaders ("User-Agent: 4GE-Updater\r\nAccept: application/vnd.github+json\r\n")
                    .withConnectionTimeoutMs (timeoutMs).withNumRedirectsToFollow (10);
        return URL (address).createInputStream (o);
    }
    static Release fetchLatest()
    {
        Release r;
        auto in = open (String ("https://api.github.com/repos/") + repo + "/releases/tags/" + tag);
        if (in == nullptr) { r.error = "no connection to GitHub"; return r; }
        const var j = JSON::parse (in->readEntireStreamAsString());
        if (auto* assets = j["assets"].getArray())
            for (auto& a : *assets)
                if (a["name"].toString() == assetName) { r.url = a["browser_download_url"].toString(); r.date = a["updated_at"].toString(); }
        if (r.url.isEmpty()) { r.error = "no 4GE package in the release"; return r; }
        const String body = j["body"].toString();
        const int c = body.indexOf ("commit ");
        if (c >= 0) { const String s = body.substring (c + 7, c + 47); if (s.containsOnly ("0123456789abcdef") && s.length() == 40) r.sha = s; }
        r.id = r.sha.isNotEmpty() ? r.sha : r.date;
        if (r.sha.isNotEmpty())
            if (auto cin = open (String ("https://api.github.com/repos/") + repo + "/commits/" + r.sha))
                r.msg = JSON::parse (cin->readEntireStreamAsString())["commit"]["message"].toString().upToFirstOccurrenceOf ("\n", false, false);
        const Time t = Time::fromISO8601 (r.date);
        if (t.toMilliseconds() > 0) r.date = t.formatted ("%Y-%m-%d %H:%M");
        r.ok = true;
        return r;
    }

    // download + unpack + swap (background thread). Returns "" or the error.
    static String install (const Release& r, std::function<void (const String&)> status)
    {
        auto tmp = File::getSpecialLocation (File::tempDirectory).getChildFile ("4GE_update");
        tmp.deleteRecursively(); tmp.createDirectory();
        auto zip = tmp.getChildFile ("4GE.zip");
        status ("downloading...");
        {
            auto in = open (r.url, 60000);
            if (in == nullptr) return "the download did not start";
            FileOutputStream out (zip);
            if (! out.openedOk()) return "cannot write " + zip.getFullPathName();
            if (out.writeFromInputStream (*in, -1) < 1000) return "the download is empty";
        }
        status ("unpacking...");
        { ZipFile z (zip); const auto res = z.uncompressTo (tmp, true); if (res.failed()) return "unzip: " + res.getErrorMessage(); }
        Array<File> found = tmp.findChildFiles (File::findDirectories, true, "4GE.vst3");
        if (found.isEmpty()) return "no 4GE.vst3 in the package";
        if (flOpen()) return "FL";
        status ("installing...");
        const File dst = vst3Target(), neu = dst.getSiblingFile ("4GE.vst3.new"), old = dst.getSiblingFile ("4GE.vst3.old");
        dst.getParentDirectory().createDirectory();
        neu.deleteRecursively(); old.deleteRecursively();
        if (! found[0].copyDirectoryTo (neu)) return "cannot write into " + dst.getParentDirectory().getFullPathName() + " (administrator rights?)";
        if (dst.exists() && ! dst.moveFileTo (old)) { neu.deleteRecursively(); return "the old 4GE.vst3 is in use (FL / a scan?)"; }
        if (! neu.moveFileTo (dst)) { old.moveFileTo (dst); return "cannot put the new 4GE.vst3 in place"; }
        old.deleteRecursively();
        tmp.deleteRecursively();
        return {};
    }

    // ---------------------------------------------------------------- the window
    struct Look : LookAndFeel_V4
    {
        Font getTextButtonFont (TextButton&, int h) override { return barlow (jmin (17.f, (float) h * 0.55f), true); }
    };
    class Panel : public Component, private Timer
    {
    public:
        std::function<void()> onCheck, onUpdate, onAuto;
        ~Panel() override { setLookAndFeel (nullptr); }
        Panel()
        {
            setLookAndFeel (&look);
            for (auto* b : { &checkBtn, &updateBtn, &autoBtn })
            {
                addAndMakeVisible (*b);
                b->setColour (TextButton::buttonColourId, col::control); b->setColour (TextButton::buttonOnColourId, col::red);
                b->setColour (TextButton::textColourOffId, col::silver); b->setColour (TextButton::textColourOnId, Colours::white);
            }
            autoBtn.setClickingTogglesState (true);
            checkBtn.onClick = [this] { if (onCheck) onCheck(); };
            updateBtn.onClick = [this] { if (onUpdate) onUpdate(); };
            autoBtn.onClick = [this] { if (onAuto) onAuto(); };
            setSize (620, 330);
            startTimerHz (2);
        }
        String installed = "-", latest = "-", latestMsg, status = "starting...", lastCheck = "-";
        Colour statusColour = col::dim;
        void setAuto (bool on) { autoBtn.setToggleState (on, dontSendNotification); autoBtn.setButtonText (on ? "AUTO: ON" : "AUTO: OFF"); }
        void paint (Graphics& g) override
        {
            g.fillAll (col::ground);
            doomsday::brand::drawBadge (g, { 18.f, 14.f, 70.f, 36.f }, 0.55f, RectanglePlacement::xLeft | RectanglePlacement::yMid);
            g.setColour (col::silver); g.setFont (barlow (26.f, true)); g.drawText ("4GE", 104, 12, 70, 30, Justification::left);
            g.setColour (col::red); g.setFont (barlow (15.f, true)); g.drawText ("UPDATER", 156, 18, 120, 20, Justification::left);
            g.setColour (col::dim); g.setFont (barlow (13.f, false));
            g.drawText ("keeps the newest 4GE test build in your VST3 folder - silently", 104, 38, 480, 16, Justification::left);
            auto card = juce::Rectangle<float> (18.f, 66.f, (float) getWidth() - 36.f, 190.f);
            g.setColour (col::panel); g.fillRoundedRectangle (card, 5.f);
            g.setColour (col::line); g.drawRoundedRectangle (card.reduced (0.5f), 5.f, 1.f);
            int y = 78;
            auto row = [&] (const String& k, const String& v, Colour c, float h = 15.f)
            {
                g.setColour (col::dim); g.setFont (barlow (12.f, true)); g.drawText (k, 32, y, 120, 20, Justification::left);
                g.setColour (c); g.setFont (barlow (h, false)); g.drawFittedText (v, 150, y, getWidth() - 182, 20, Justification::left, 1, 0.8f);
                y += 26;
            };
            row ("STATUS", status, statusColour, 17.f);
            row ("INSTALLED", installed, col::silver);
            row ("ON GITHUB", latest, col::silver);
            row ("CHANGE", latestMsg.isEmpty() ? "-" : latestMsg, col::dim, 13.f);
            row ("LAST CHECK", lastCheck, col::dim, 13.f);
            row ("FL STUDIO", flOpen() ? "open (4GE is swapped after FL closes)" : "closed", col::dim, 13.f);
            g.setColour (col::dim.withAlpha (0.7f)); g.setFont (barlow (11.f, false));
            g.drawText ("closing the window keeps it running in the tray (right-click the tray icon: QUIT)", 18, getHeight() - 20, getWidth() - 36, 14, Justification::left);
        }
        void resized() override
        {
            const int y = 268, h = 30;
            checkBtn.setBounds (18, y, 150, h); updateBtn.setBounds (176, y, 170, h); autoBtn.setBounds (getWidth() - 18 - 130, y, 130, h);
        }
    private:
        void timerCallback() override { repaint(); }
        Look look;
        TextButton checkBtn { "CHECK NOW" }, updateBtn { "UPDATE NOW" }, autoBtn { "AUTO: ON" };
    };

    class Window : public DocumentWindow
    {
    public:
        explicit Window (Panel* p) : DocumentWindow ("4GE Updater", col::ground, DocumentWindow::closeButton | DocumentWindow::minimiseButton)
        {
            setUsingNativeTitleBar (true);
            setContentNonOwned (p, true);
            centreWithSize (getWidth(), getHeight());
            setResizable (false, false);
        }
        void closeButtonPressed() override { setVisible (false); }     // back to the tray
    };

    // ---------------------------------------------------------------- the tray + the logic
    class App;
    class Tray : public SystemTrayIconComponent
    {
    public:
        std::function<void()> onOpen, onCheck, onQuit;
        Tray()
        {
            Image img (Image::ARGB, 32, 32, true);
            {
                Graphics g (img);
                g.setColour (col::ground); g.fillRoundedRectangle (0.f, 0.f, 32.f, 32.f, 6.f);
                g.setColour (col::red); g.setFont (barlow (19.f, true)); g.drawText ("4GE", 0, 0, 32, 32, Justification::centred);
            }
            setIconImage (img, img);
            setIconTooltip ("4GE Updater");
        }
        void mouseDown (const MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                PopupMenu m;
                m.addItem ("Open 4GE Updater", [this] { if (onOpen) onOpen(); });
                m.addItem ("Check now", [this] { if (onCheck) onCheck(); });
                m.addSeparator();
                m.addItem ("Quit (no more updates until the next logon)", [this] { if (onQuit) onQuit(); });
                m.showMenuAsync (PopupMenu::Options());
            }
            else if (onOpen) onOpen();
        }
    };

    class Controller : private Timer, private Thread
    {
    public:
        Controller (bool background) : Thread ("4GE update")
        {
            PropertiesFile::Options o; o.applicationName = "settings"; o.filenameSuffix = ".xml"; o.folderName = "4GE_Updater";
            settings = std::make_unique<PropertiesFile> (dataDir().getChildFile ("settings.xml"), o);
            panel.onCheck = [this] { request (false); };
            panel.onUpdate = [this] { request (true); };
            panel.onAuto = [this] { settings->setValue ("auto", ! autoOn()); settings->saveIfNeeded(); panel.setAuto (autoOn()); };
            panel.setAuto (autoOn());
            panel.installed = installedText();
            window = std::make_unique<Window> (&panel);
            tray.onOpen = [this] { show(); };
            tray.onCheck = [this] { request (false); };
            tray.onQuit = [] { JUCEApplication::getInstance()->systemRequestedQuit(); };
            if (! background) show();
            log ("start" + String (background ? " (background)" : ""));
            startTimer (1000);                                   // a second tick: the 10-minute check + "FL closed now -> swap"
        }
        ~Controller() override { stopTimer(); signalThreadShouldExit(); notify(); stopThread (60000); }
        void show() { window->setVisible (true); window->toFront (true); }
    private:
        bool autoOn() const { return settings->getBoolValue ("auto", true); }
        String installedText() const
        {
            const String d = settings->getValue ("installedDate"), m = settings->getValue ("installedMsg");
            return settings->getValue ("installed").isEmpty() ? String ("unknown (not installed by the updater yet)") : d + (m.isNotEmpty() ? "  -  " + m : String());
        }
        void request (bool forceInstall) { wantInstall = forceInstall || wantInstall.load(); wantCheck = true; if (! isThreadRunning()) startThread(); else notify(); }
        void timerCallback() override
        {
            const auto now = Time::getMillisecondCounter();
            if (lastAuto == 0 || now - lastAuto > 10u * 60u * 1000u) { lastAuto = now; request (false); }
            if (pendingId.isNotEmpty() && autoOn() && ! flOpen() && ! busy) request (true);   // waited for FL: swap now
        }
        void setStatus (const String& s, Colour c)
        {
            MessageManager::callAsync ([this, s, c] { panel.status = s; panel.statusColour = c; tray.setIconTooltip ("4GE Updater - " + s); panel.repaint(); });
        }
        void run() override
        {
            while (! threadShouldExit())
            {
                if (wantCheck.exchange (false))
                {
                    busy = true;
                    const bool force = wantInstall.exchange (false);
                    setStatus ("checking GitHub...", col::dim);
                    const Release r = fetchLatest();
                    MessageManager::callAsync ([this, r] { panel.lastCheck = Time::getCurrentTime().formatted ("%H:%M:%S");
                                                           if (r.ok) { panel.latest = r.date; panel.latestMsg = r.msg; } panel.repaint(); });
                    if (! r.ok) { setStatus ("cannot check: " + r.error, col::amber); log ("check: " + r.error); }
                    else if (r.id == settings->getValue ("installed") && ! force) { setStatus ("up to date", col::green); pendingId.clear(); }
                    else if (! autoOn() && ! force) { setStatus ("a new build is ready - press UPDATE NOW", col::amber); }
                    else if (flOpen()) { setStatus ("a new build is waiting - it is installed when FL Studio closes", col::amber); pendingId = r.id; }
                    else
                    {
                        const String err = install (r, [this] (const String& s) { setStatus (s, col::dim); });
                        if (err.isEmpty())
                        {
                            MessageManager::callAsync ([this, r]
                            {
                                settings->setValue ("installed", r.id); settings->setValue ("installedDate", r.date); settings->setValue ("installedMsg", r.msg);
                                settings->saveIfNeeded(); panel.installed = installedText(); panel.repaint();
                            });
                            pendingId.clear();
                            setStatus ("4GE updated (" + r.date + ")", col::green);
                            log ("installed " + r.id + " (" + r.date + ") " + r.msg);
                        }
                        else if (err == "FL") { setStatus ("a new build is waiting - it is installed when FL Studio closes", col::amber); pendingId = r.id; }
                        else { setStatus ("not installed: " + err, col::red); log ("install failed: " + err); }
                    }
                    busy = false;
                }
                wait (-1);
            }
        }
        Panel panel;
        std::unique_ptr<Window> window;
        Tray tray;
        std::unique_ptr<PropertiesFile> settings;
        std::atomic<bool> wantCheck { false }, wantInstall { false }, busy { false };
        String pendingId;
        uint32 lastAuto = 0;
    };

    class App : public JUCEApplication
    {
    public:
        const String getApplicationName() override { return "4GE Updater"; }
        const String getApplicationVersion() override { return "1.0.0"; }
        bool moreThanOneInstanceAllowed() override { return false; }
        void initialise (const String& cmd) override
        {
            if (cmd.contains ("--snapshot"))                       // (development: a PNG of the window, then quit)
            {
                Panel p; p.status = "up to date"; p.statusColour = col::green; p.installed = "2026-11-24 19:52  -  4GE HARDCORE profile rebuilt from the owner's ref";
                p.latest = "2026-11-24 19:52"; p.latestMsg = "4GE HARDCORE profile rebuilt from the owner's ref"; p.lastCheck = "20:01:12"; p.setAuto (true);
                auto img = p.createComponentSnapshot (p.getLocalBounds(), true, 2.f);
                FileOutputStream os (File (cmd.fromFirstOccurrenceOf ("--snapshot", false, false).trim().unquoted()));
                if (os.openedOk()) { os.setPosition (0); os.truncate(); PNGImageFormat().writeImageToStream (img, os); }
                quit(); return;
            }
            controller = std::make_unique<Controller> (cmd.contains ("--background"));
        }
        void shutdown() override { controller.reset(); }
        void anotherInstanceStarted (const String&) override { if (controller) controller->show(); }   // the desktop shortcut: open the window
        void systemRequestedQuit() override { quit(); }
    private:
        std::unique_ptr<Controller> controller;
    };
}

START_JUCE_APPLICATION (upd::App)
