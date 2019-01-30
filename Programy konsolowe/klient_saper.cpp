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

void rysujGre() {
	int row, col;
	
	printf("\n\nBiezaca gra:\n");
	
	if(czyWPokoju) {
		printf("Liczba min do oznaczenia:\t%d\n", pokoj.liczbaMinDoOznaczenia);
		for(row = 0; row < pokoj.wysokoscPlanszy; row++) {
			for(col = 0; col < pokoj.szerokoscPlanszy; col++) {
				if(pokoj.stanPlanszy[row][col] == 0) {
					//printf("%c", char(219));
					printf("X");
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
			printf("\n");
		}
		
		
	} else {
		printf("Jestes w menu glownym.\n");
	}
}

void czytajZSerwera(int sock)
{
	int buffsize = 255, received;
	char buffer[buffsize], tmpBuffer[buffsize];
	int poczatek, koniec, count = 0;
	
	int kod, row, col, id, nowaWysokoscPlanszy, nowaSzerokoscPlanszy, nowaLiczbaMinDoOznaczenia;
	char *tresc;
	
	pokoj.wysokoscPlanszy = 0;
	pokoj.szerokoscPlanszy = 0;
	czyWPokoju = false;
	
	// read from socket, write to stdout
    while(true)
    {
		rysujGre();
		
        received = odbierzDane(sock, buffer, buffsize);
		buffer[received] = '\0';
		
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
				printf("(count = %d):\t|%s|\n", count, tmpBuffer);
				
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
					nowaLiczbaMinDoOznaczenia = strtol(tresc, &tresc, 10);
					
					
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
						printf("Trwa usuwanie stanu planszy...\n");
						for(row = 0; row < pokoj.wysokoscPlanszy; row++) {
							delete[]pokoj.stanPlanszy[row];
						}
						delete[]pokoj.stanPlanszy;
						
						printf("Trwa usuwanie planszy...\n");
						for(row = 0; row < pokoj.wysokoscPlanszy; row++) {
							delete[]pokoj.plansza[row];
						}
						delete[]pokoj.plansza;
					}
					
					pokoj.wysokoscPlanszy = nowaWysokoscPlanszy;
					pokoj.szerokoscPlanszy = nowaSzerokoscPlanszy;
					pokoj.liczbaMinDoOznaczenia = nowaLiczbaMinDoOznaczenia;
					
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
					
					czyWPokoju = true;
				}
			}
		}
    }
}

void wysylajDoSerwera(int sock)
{
	int buffsize = 255, received;
	char buffer[buffsize];
	
	// read from stdin, write to socket
    while(true)
    {
        received = odbierzDane(0, buffer, buffsize);
        wyslijDane(sock, buffer, received);
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
	if(sock == -1) error(1, errno, "socket failed");
	
	// attept to connect
	res = connect(sock, resolved->ai_addr, resolved->ai_addrlen);
	if(res) error(1, errno, "connect failed");
	
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
