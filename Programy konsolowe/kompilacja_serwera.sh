# Należy upewnić się, że znak końca lini jest odpowiednio ustawiony - Unix(LF)
echo 'Kompilacja pliku serwer.cpp w toku...'
g++ --std=c++17 -Wall -O0 -g -pthread -o serwer serwer.cpp
echo 'Kompilacja ukończona.'