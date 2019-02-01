#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <error.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <poll.h> 
#include <thread>
#include <stdio.h>

struct room {
	char nazwa[256];
	int id, liczbaGraczy, gracze[16];
	int **plansza, **stanPlanszy, wysokoscPlanszy, szerokoscPlanszy;
	int liczbaMin, liczbaMinDoOznaczenia, stanGry, liczbaNieodkrytychPol;
};

room pokoj;
bool czyWPokoju;


int odbierzDane(int fd, char *buffer, int buffsize){
	int ret = read(fd, buffer, buffsize);
	if(ret==-1) error(1,errno, "Blad funkcji read() na deskryptorze %d", fd);
	return ret;
}

void wyslijDane(int fd, char *buffer, int count){
	int ret = write(fd, buffer, count);
	if(ret == -1) error(1, errno, "Blad funkcji write() na deskryptorze %d", fd);
	if(ret != count) error(0, errno, "Blad funkcji write() na deskryptorze %d - zapisano mniej danych niz oczekiwano: (%d/%d)", fd, count, ret);
}

int podajLiczbeCyfr(int x) {
	if(x == 0) {
		return 1;
	}
	
	int wynik = 0;
	
	while(x) {
		x /= 10;
		wynik++;
	}
	
	return wynik;
}

int podajNtaCyfre(int x, int n) {
	int wynik = x % 10;
	
	if((x == 0) && (n == 1)) {
		return 0;
	}
	
	while(n != 1) {
		x /= 10;
		wynik = x % 10;
		n--;
	}
	
	if((wynik == 0) && (x == 0)) {
		return -1;
	}
	
	return wynik;
}

void rysujDostepneKomendy() {
	printf("\n\n//---------------------------------------- Dostepne akcje ----------------------------------------\\\\\n");
	
	if(czyWPokoju) {
		if(pokoj.stanGry == 1) {
			printf("--> (\"04 %%d %%d\", numer wiersza, numer kolumny) - oznacz flaga wskazane pole\n");
			printf("--> (\"05 %%d %%d\", numer wiersza, numer kolumny) - oznacz znakiem zapytania wskazane pole\n");
			printf("--> (\"06 %%d %%d\", numer wiersza, numer kolumny) - ustaw wskazane pole w stanie nieoznaczenia\n");
			printf("--> (\"07 %%d %%d\", numer wiersza, numer kolumny) - odkryj wskazane pole\n");
			printf("--> (\"11 %%d %%d\", numer wiersza, numer kolumny) - odkryj okolice wskazanego pola na podstawie oznaczonych dookola flag\n\n");
		}
		printf("--> (\"08\") - opusc pokoj gry\n");
		printf("--> (\"09\") - zrestartuj plansze\n");
		printf("--> (\"10 %%d %%d %%d\", wysokosc planszy, szerokosc planszy, liczba min) - rozpocznij gre z nowymi parametrami\n");
	} else {
		printf("--> (\"01 %%s\", nazwa pokoju) - zagraj w stworzonym przez Ciebie pokoju o podanej nazwie\n");
		printf("--> (\"02\") - odswiez liste dostepnych pokoi gier\n");
		printf("--> (\"03 %%d\", id pokoju) - dolacz do pokoju gry o wskazanym id pokoju\n");
	}
	
	printf("\\\\------------------------------------------------------------------------------------------------//\n");
}

void rysujGre() {
	int row, col, liczbaCyfr, cyfra;
	
	liczbaCyfr = podajLiczbeCyfr(pokoj.szerokoscPlanszy - 1);
	
	system("clear");
	
	printf("Liczba min do oznaczenia:\t%d\n\n", pokoj.liczbaMinDoOznaczenia);
	
	for(row = liczbaCyfr; row > 0; row--) {
		printf("      ");
		for(col = 0; col < pokoj.szerokoscPlanszy; col++) {
			cyfra = podajNtaCyfre(col, row);
			if(cyfra != -1) {
				printf("%d", cyfra);
			} else {
				printf(" ");
			}
		}
		printf("\n");
	}
	
	printf("     \u250C");
	for(col = 0; col < pokoj.szerokoscPlanszy; col++) {
		printf("\u2500");
	}
	printf("\u2510\n");
	
	for(row = 0; row < pokoj.wysokoscPlanszy; row++) {
		if(row == 0) {
			printf("    0\u2502");
		} else {
			printf("%5.d\u2502", row);
		}
		
		for(col = 0; col < pokoj.szerokoscPlanszy; col++) {
			if(pokoj.stanPlanszy[row][col] == 0) {
				printf("\u2588");
			} else if(pokoj.stanPlanszy[row][col] == 1) {
				printf("!");
			} else if(pokoj.stanPlanszy[row][col] == 2) {
				printf("?");
			} else if(pokoj.plansza[row][col] == -1) {
				printf("*");
			} else if(pokoj.plansza[row][col] == 0) {
				printf(" ");
			} else {
				printf("%d", pokoj.plansza[row][col]);
			}
		}
		
		printf("\u2502\n");
	}
	
	printf("     \u2514");
	for(col = 0; col < pokoj.szerokoscPlanszy; col++) {
		printf("\u2500");
	}
	printf("\u2518\n");
	
	czyWPokoju = true;
	
	rysujDostepneKomendy();
}

void rysujMenuGlowne() {
	system("clear");
	
	printf("//-- menu glowne --\\\\\n\n");
	
	printf("Lista dostepnych pokoi gier:\n");
	
	czyWPokoju = false;
}

void czytajZSerwera(int sock)
{
	int buffsize = 255, received;
	char buffer[buffsize], tmpBuffer[buffsize];
	int poczatek, koniec, count = 0;
	
	int kod, row, col, id, nowaWysokoscPlanszy, nowaSzerokoscPlanszy;
	char *tresc;
	
	pokoj.wysokoscPlanszy = 0;
	pokoj.szerokoscPlanszy = 0;
	
	// read from socket, write to stdout
    while(true)
    {
        received = odbierzDane(sock, buffer, buffsize);
		buffer[received] = '\0';
		
		if(received == 0) {
			system("clear");
			printf("Serwer zostal wylaczony.\n");
			printf("Trwa zamykanie programu...\n");
			
			close(sock);
			error(1,0,"Serwer zostal wylaczony.\nKoniec gry.\n");
		}
		
		poczatek = 0;
		koniec = 0;
		while(koniec != received) {
			// poszukiwanie znaku konca linii
			while((koniec != received) && (buffer[koniec] != '\n')) {
				koniec++;
			}
			
			// kopiowanie komendy do lancucha tmpBuffer
			while(poczatek != koniec) {
				tmpBuffer[count] = buffer[poczatek];
				poczatek++;
				count++;
			}
			
			if(koniec != received) {
				// znaleziono znak konca linii -> komenda jest kompletna
				tmpBuffer[count] = '\0';
				//printf("(count = %d):\t|%s|\n", count, tmpBuffer);
				
				// przygotowanie indeksow do wydzielenia kolejnej komendy
				count = 0;
				koniec++;
				poczatek = koniec;
				
				
				tresc = tmpBuffer;
				kod = strtol(tresc, &tresc, 10);
				if(kod == 1) {
					// serwer przesyla informacje o dostepnym pokoju
					id = strtol(tresc, &tresc, 10);
					printf("%d:\t%s\n", id, tresc);
					
					
				} else if(kod == 2) {
					// serwer przesyla informacje o stanie gry
					pokoj.stanGry = strtol(tresc, &tresc, 10);
					
					
				} else if(kod == 3) {
					// serwer przesyla informacje o rozmiarze planszy
					nowaWysokoscPlanszy = strtol(tresc, &tresc, 10);
					nowaSzerokoscPlanszy = strtol(tresc, &tresc, 10);
					
					
				} else if(kod == 4) {
					// serwer przesyla ilosc min pozostala do odznaczenia
					pokoj.liczbaMinDoOznaczenia = strtol(tresc, &tresc, 10);
					
					
				} else if(kod == 5) {
					// serwer przesyla informacje o stanie w jakim znajduje sie pole planszy
					row = strtol(tresc, &tresc, 10);
					col = strtol(tresc, &tresc, 10);
					pokoj.stanPlanszy[row][col] = strtol(tresc, &tresc, 10);
					
					
				} else if(kod == 6) {
					//serwer przesyla informacje o odkrytym polu planszy
					row = strtol(tresc, &tresc, 10);
					col = strtol(tresc, &tresc, 10);
					pokoj.plansza[row][col] = strtol(tresc, &tresc, 10);
					pokoj.stanPlanszy[row][col] = 3;
					
					
				} else if(kod == 7) {
					// serwer przesyla polecenia rozpoczecia nowej gry przy obecnych parametrach
					
					if((pokoj.wysokoscPlanszy != 0) || (pokoj.szerokoscPlanszy != 0)) {
						// trzeba zwolnic pamiec po pozostalej planszy
						for(row = 0; row < pokoj.wysokoscPlanszy; row++) {
							delete[]pokoj.stanPlanszy[row];
						}
						delete[]pokoj.stanPlanszy;
						
						for(row = 0; row < pokoj.wysokoscPlanszy; row++) {
							delete[]pokoj.plansza[row];
						}
						delete[]pokoj.plansza;
					}
					
					pokoj.wysokoscPlanszy = nowaWysokoscPlanszy;
					pokoj.szerokoscPlanszy = nowaSzerokoscPlanszy;
					
					// stworzenie dwuwymiarowych tabel dla stanu planszy i wartosci pol planszy
					pokoj.stanPlanszy = new int*[pokoj.wysokoscPlanszy];
					for(row = 0; row < pokoj.wysokoscPlanszy; row++) {
						pokoj.stanPlanszy[row] = new int[pokoj.szerokoscPlanszy];
						for(col = 0; col < pokoj.szerokoscPlanszy; col++) {
							pokoj.stanPlanszy[row][col] = 0;
						}
					}
					
					pokoj.plansza = new int*[pokoj.wysokoscPlanszy];
					for(row = 0; row < pokoj.wysokoscPlanszy; row++) {
						pokoj.plansza[row] = new int[pokoj.szerokoscPlanszy];
						for(col = 0; col < pokoj.szerokoscPlanszy; col++) {
							pokoj.plansza[row][col] = 0;
						}
					}
					
					pokoj.stanGry = 1;
					
					
				} else if(kod == 8) {
					// serwer daje znac, ze przeslal juz wszystkie informacje na temat konsekwencji wykonanego ostatnio ruchu
					rysujGre();
					
					
				} else if(kod == 9) {
					// serwer daje znac, ze klient znajduje sie w menu glownym gry
					rysujMenuGlowne();
					
					
				} else if(kod == 10) {
					// serwer daje znac, ze zakonczyl juz przesylanie listy dostepnych serwerow
					rysujDostepneKomendy();
				}
			}
		}
    }
}

void wysylajDoSerwera(int sock)
{
	int buffsize = 255, received, kod;
	char buffer[buffsize];
	
	// read from stdin, write to socket
    while(true)
    {
        received = odbierzDane(0, buffer, buffsize);
        wyslijDane(sock, buffer, received);
		
		kod = strtol(buffer, NULL, 10);
		if(kod == 8) {
			czyWPokoju = false;
		}
    }
}

int main(int argc, char ** argv){
	if(argc!=2) error(1,0,"Podaj jako argument adres serwera gry \"Saper\"");
	
	// Resolve arguments to IPv4 address with a port number
	addrinfo *resolved, hints={.ai_flags=0, .ai_family=AF_INET, .ai_socktype=SOCK_STREAM};
	int res = getaddrinfo(argv[1], "61596", &hints, &resolved);
	if(res || !resolved) error(1, 0, "getaddrinfo: %s", gai_strerror(res));
	
	// create socket
	int sock = socket(resolved->ai_family, resolved->ai_socktype, 0);
	if(sock == -1) error(1, errno, "Blad podczas socket()");
	
	// attept to connect
	res = connect(sock, resolved->ai_addr, resolved->ai_addrlen);
	if(res) error(1, errno, "Blad podczas connect()");
	
	// free memory
	freeaddrinfo(resolved);
    
    std::thread wysylaj, czytaj;
    
    wysylaj = std::thread(wysylajDoSerwera, sock);
    czytaj = std::thread(czytajZSerwera, sock);
    
    wysylaj.join();
    czytaj.join();
	

	close(sock);
	
	return 0;
}
