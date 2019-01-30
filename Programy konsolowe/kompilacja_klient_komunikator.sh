# Należy upewnić się, że znak końca lini jest odpowiednio ustawiony - Unix(LF)
echo 'Kompilacja pliku klient_komunikator.cpp w toku...'
g++ --std=c++17 -Wall -O0 -g -pthread -o klient_komunikator klient_komunikator.cpp
echo 'Kompilacja ukończona.'