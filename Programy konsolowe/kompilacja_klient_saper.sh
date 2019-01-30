# Należy upewnić się, że znak końca lini jest odpowiednio ustawiony - Unix(LF)
echo 'Kompilacja pliku klient_saper.cpp w toku...'
g++ --std=c++17 -Wall -O0 -g -pthread -o klient_saper klient_saper.cpp
echo 'Kompilacja ukończona.'