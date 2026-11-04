<!-- Owner specification of module 12 (2026-11-04), collected from chat piece by piece. Plan: PLAN.md 1l. -->
# 4GE — MODULE 12: MASTER OUTPUT & EXPORT ENGINE

## 1. CEL MODUŁU
Master Output & Export Engine odpowiada za końcowy sygnał wyjściowy instrumentu, kontrolę poziomu, mierniki oraz eksport gotowych kicków do plików audio.
Moduł nie zastępuje procesorów dynamiki z Module 08 – zarządza wyjściem i prawidłowym renderowaniem dźwięku.
Najważniejsze cele: profesjonalny master output; dokładne mierniki poziomu; pomiar True Peak; pomiar RMS i LUFS; eksport pojedynczych kicków; eksport wybranych grup i warstw; Batch Export; Offline Rendering; kontrola formatu i jakości plików; powtarzalność renderowania.
WAŻNE: wtyczka nie może automatycznie zmieniać głośności, normalizować ani limitować sygnału bez wiedzy użytkownika.
