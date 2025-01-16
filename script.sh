#!/bin/sh

echo "=== [1] Tworzenie dysku wirtualnego (5 MB) ==="
sleep 1
echo "--- create disk 5242880 ---"
./manager create disk 5242880
sleep 5

echo
echo "=== [1a] Mapa świeżo utworzonego dysku ==="
echo "--- map ---"
sleep 1
./manager map disk
sleep 5

echo
echo "=== [2] Generujemy 10 plików (~500 KB każdy) ==="
sleep 1
for i in `seq 1 11`; do
    echo "Tworzenie file${i}.bin (500 KB)"
    ./mkfile file${i}.bin 512000
done
sleep 5

echo
echo "=== [3] Wgrywamy 10 plików na dysk (file1.bin ... file10.bin) ==="
sleep 1
for i in `seq 1 10`; do
    echo "--- copyin file${i}.bin => 'file${i}.bin' ---"
    ./manager copyin disk file${i}.bin file${i}.bin
    sleep 1
done
sleep 5

echo
echo "=== [3a] Probujemy wgrac plik ktory sie nie juz zmiesci ==="
sleep 1
echo "--- copyin file11.bin => 'file11.bin' ---"
./manager copyin disk file11.bin file11.bin
sleep 5

echo
echo "=== [3b] Mapa dysku po wgraniu 10 plików ==="
sleep 1
echo "--- map ---"
./manager map disk
sleep 5

echo
echo "=== [4] Usuwamy pliki file2.bin, file5.bin i file8.bin, tworząc kilka dziur ==="
sleep 1
echo "--- rm disk file2.bin ---"
./manager rm disk file2.bin
sleep 1
echo "--- rm disk file5.bin ---"
./manager rm disk file5.bin
sleep 1
echo "--- rm disk file8.bin ---"
./manager rm disk file8.bin
sleep 5

echo
echo "=== [4a] Mapa dysku (powinny być 'dziury' w miejscach po skasowanych plikach) ==="
sleep 1
echo "--- map ---"
./manager map disk
sleep 5

echo
echo "=== [5] Generujemy plik bigfile.bin (ok. 1.6 MB) i wgrywamy go na dysk ==="
sleep 1
./mkfile bigfile.bin 1600000
sleep 2

echo
echo "--- copyin bigfile.bin => 'bigfile.bin' ---"
sleep 1
./manager copyin disk bigfile.bin bigfile.bin
sleep 5

echo
echo "=== [5a] Mapa dysku (bigfile.bin powinien być w kilku fragmentach) ==="
sleep 1
echo "--- map ---"
./manager map disk
sleep 5

echo
echo "=== [6] Lista plików na dysku (ls) ==="
sleep 1
echo "--- ls ---"
./manager ls disk
sleep 5

echo
echo "=== [7] Usuwamy bigfile.bin (zwolni sporo bloków) ==="
sleep 1
echo "--- rm disk bigfile.bin ---"
./manager rm disk bigfile.bin
sleep 5

echo
echo "=== Mapa dysku po usunięciu bigfile.bin ==="
echo "--- map ---"
sleep 1
./manager map disk
sleep 5

echo
echo "=== [8] Tworzymy plik ukryty (.secret 1KB) i wgrywamy go na dysk ==="
sleep 1
./mkfile .secret 1024
echo "--- copy .secret => '.secret' ---"
./manager copyin disk .secret .secret
sleep 5
echo
echo "--- ls ---"
./manager ls disk
sleep 5
echo
echo "--- ls -a ---"
./manager "ls -a" disk
sleep 5
echo "Usuwamy plik ukryty (.secret)"
echo
echo "--- rm disk .secret ---"
./manager rm disk .secret
sleep 5
echo
echo "--- map ---"
./manager map disk
sleep 5


echo
echo "=== [9] Obsluga dlugich nazw plikow ze spacjami ==="
sleep 1
echo "--- copyin 'manager.c' => 'systemy operacyjne skrypt demonstracyjny.bin' ---"
./manager copyin disk manager.c "systemy operacyjne skrypt demonstracyjny.bin"
sleep 5
echo
echo "--- ls ---"
./manager ls disk
sleep 5

echo
echo "=== [10] Kopiowanie z wirtualnego dysku na minix ==="
sleep 1
echo "--- copyout disk "systemy operacyjne skrypt demonstracyjny.bin" "systemy operacyjne skrypt demonstracyjny.bin" ---"
./manager copyout disk "systemy operacyjne skrypt demonstracyjny.bin" "systemy operacyjne skrypt demonstracyjny.bin"
sleep 5

echo 
echo "=== [11] Sprawdzenie czy pliki skopiowane z wirtualnego dysku sa takie same jak oryginalne ==="
sleep 1
echo "--- diff manager.c "systemy operacyjne skrypt demonstracyjny.bin" ---"
diff manager.c "systemy operacyjne skrypt demonstracyjny.bin"
sleep 5


echo
echo "=== [12] Usuwamy cały dysk ==="
sleep 1
echo "--- rmdisk disk ---"
./manager rmdisk disk
sleep 5

echo
echo "=== Sprzatanie ==="
echo "--- rm -f file*.bin bigfile.bin .secret systemy operacyjne skrypt demonstracyjny.bin ---"
rm -f file*.bin bigfile.bin .secret "systemy operacyjne skrypt demonstracyjny.bin"

echo
echo "=== Koniec demonstracji ==="
