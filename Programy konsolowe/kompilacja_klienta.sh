# Należy upewnić się, że znak końca lini jest odpowiednio ustawiony - Unix(LF)
echo 'Kompilacja pliku klient.cpp w toku...'
g++ --std=c++17 -Wall -O0 -g -pthread -o klient klient.cpp
echo 'Kompilacja ukończona.'