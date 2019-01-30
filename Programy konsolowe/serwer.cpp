#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <error.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <poll.h> 
#include <thread>
#include <unordered_set>
#include <list>
#include <stack>
#include <queue>
#include <signal.h>
#include <cstring>
#include <time.h>

struct room {
	char nazwa[256];
	int id, liczbaGraczy, gracze[16];
	int **plansza, **stanPlanszy, wysokoscPlanszy, szerokoscPlanszy;
	int liczbaMin, liczbaMinDoOznaczenia, stanGry, liczbaNieodkrytychPol;
};

// server socket
int servFd;

// client sockets
std::unordered_set<int> clientFds;

// przechowywanie informacji o aktywnych grach
//std::vector<room> pokojeGier;
std::list<room> pokojeGier;
int liczbaGier = 0;

// handles SIGINT
void ctrl_c(int);

// sends data to clientFds excluding fd
void sendToAllBut(int fd, char * buffer, int count);

// converts cstring to port
uint16_t readPort(char * txt);

// sets SO_REUSEADDR
void setReuseAddr(int sock);

// przypisuje do danego pokoju podanego gracza
void dodajGraczaDoPokoju(room &pokoj, int clientFd);

// ustawia parametry gry dla danego pokoju
void ustawNowaGre(room &pokoj, int wysokoscPlanszy, int szerokoscPlanszy, int liczbaMin);

void zakonczWiadomoscZnakiemKoncaLini(char *buffer);

void itoa(int liczba, char *tekst, int podstawa);

void wyslijLiczby(int clientFd, int *tablica, int n, char *buffer, char *tmpBuffer);

void wyslijDoWszystkich(room *biezacyPokoj, char *buffer, int count);

void wyslijLiczbyDoWszystkich(room *biezacyPokoj, int *tablica, int n, char *buffer, char *tmpBuffer);

void odkryjPlanszeFloodFill(room *biezacyPokoj, int row, int col, int *tablica, char *buffer, char *tmpBuffer);

void odkryjBomby(room *biezacyPokoj, int *tablica, char *buffer, char *tmpBuffer);

void wyslijDane(int fd, char *buffer, int count);

void odczytajWiadomosc(int clientFd, sockaddr_in clientAddr)
{
	long long int kod;
	room tmpPokoj, *biezacyPokoj = NULL;
	char buffer[256], tmpBuffer[256], *tresc;
	int count, i, j, tablica[10], row, col;
	
	while(true)
	{
		// odczytywanie wiadomosci od klienta
		count = read(clientFd, buffer, 255);
		i = count - 1;
		while((i >= 0) && (buffer[i] == '\n')) {
			buffer[i] = '\0';
			i--;
		}

		printf("\n//-------------------- ");
		printf("(fd: %d) Nowy komunikat od: %s:%hu o dlugosci %d\n", clientFd, inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), count);
		printf("%s\n", buffer);
		printf("\\\\--------------------\n");

		if(count < 1) {
			printf("Usuwanie klienta %d\n", clientFd);
			clientFds.erase(clientFd);
			close(clientFd);
			break;
		} else if(count >= 3) {
			kod = strtol(buffer, &tresc, 10);

			
			if(kod == 1) {
				// klient prosi o utworzenie nowego pokoju gry
				while(tresc[0] == ' ') {
					//tresc = &tresc[1];
					tresc += sizeof(char);
				}
				printf("Trwa dodawanie pokoju o nazwie: |%s|...\n", tresc);
				
				if(biezacyPokoj != NULL) {
					printf("Blad - nie mozna utworzyc nowego pokoju, skoro nadal sie gra w jednym z pokojow.\n");
					continue;
				}
				
				strcpy(tmpPokoj.nazwa, tresc);
				tmpPokoj.id = liczbaGier;
				tmpPokoj.liczbaGraczy = 0;
				
				ustawNowaGre(tmpPokoj, 10, 10, 10);
				dodajGraczaDoPokoju(tmpPokoj, clientFd);
				
				pokojeGier.push_back(tmpPokoj);
				biezacyPokoj = &(pokojeGier.back());
				liczbaGier++;
				
				/*printf("nazwa pokoju przed: |%s|\n", biezacyPokoj->nazwa);
				printf("nazwa pokoju w globalnej liscie przed: |%s|\n", pokojeGier.back().nazwa);
				strcpy(biezacyPokoj->nazwa, "lololo");
				printf("nazwa pokoju po zmianie: |%s|\n", biezacyPokoj->nazwa);
				printf("nazwa pokoju w globalnej liście po: |%s|\n", pokojeGier.back().nazwa);*/
				
				// wyslanie informacji o wysokosci i szerokosci planszy
				strcpy(buffer, "03");
				tablica[0] = biezacyPokoj->wysokoscPlanszy;
				tablica[1] = biezacyPokoj->szerokoscPlanszy;
				wyslijLiczby(clientFd, tablica, 2, buffer, tmpBuffer);
				
				// wyslanie informacji o pozostalej liczbie min do odznaczenia
				strcpy(buffer, "04");
				tablica[0] = biezacyPokoj->liczbaMinDoOznaczenia;
				wyslijLiczby(clientFd, tablica, 1, buffer, tmpBuffer);
				
				// wyslanie polecenia o rozpoczeciu nowej gry
				strcpy(buffer, "07");
				wyslijLiczby(clientFd, tablica, 0, buffer, tmpBuffer);
				
				printf("Zakonczono dodawanie pokoju.\n");
				
				
			} else if(kod == 2) {
				// klient prosi o otrzymanie spisu dostepnych pokoi gier
				strcpy(buffer, "01 ");
				printf("Trwa wysylanie listy pokoi...\n");
				
				if(biezacyPokoj != NULL) {
					printf("Blad - nie mozna pytac sie o spis dostepnych pokoi, skoro nadal sie gra w jednym z pokojow.\n");
					continue;
				}
				
				std::list<room>::iterator it;
				for(it = pokojeGier.begin(); it != pokojeGier.end(); it++) {
					// zapisanie do wiadomosci id pokoju
					itoa(it->id, tmpBuffer, 10);
					strcpy(&buffer[3], tmpBuffer);
					
					// zapisanie do wiadomosci nazwy pokoju
					count = strlen(buffer);
					buffer[count] = ' ';
					strcpy(&buffer[count + 1], it->nazwa);
					
					// zakonczenie wiadomosci znakiem konca lini
					zakonczWiadomoscZnakiemKoncaLini(buffer);
					
					// wyslanie wiadomosci
					count = strlen(buffer);
					//write(clientFd, buffer, count);
					wyslijDane(clientFd, buffer, count);
				}
				printf("Zakonczono wysylanie listy pokoi.\n");
				
				
			} else if(kod == 3) {
				// klient prosi o dolaczenie go do wskazanego pokoju (o danym id)
				i = strtol(tresc, NULL, 10);
				printf("Trwa dolaczanie do wybranego pokoju...\n");
				
				if(biezacyPokoj != NULL) {
					printf("Blad - nie mozna dolaczac sie do pokoju, skoro nadal sie gra w jednym z pokojow.\n");
					continue;
				}
				
				std::list<room>::iterator it;
				for(it = pokojeGier.begin(); it != pokojeGier.end(); it++) {
					if(it->id == i) {
						break;
					}
				}
				
				if(it == pokojeGier.end()) {
					printf("Blad - klient %d prosi o nieistniejacy pokoj.\n", clientFd);
					continue;
				}
				
				dodajGraczaDoPokoju(*it, clientFd);
				biezacyPokoj = &(*it);
				
				// wyslanie informacji o stanie gry
				strcpy(buffer, "02");
				tablica[0] = biezacyPokoj->stanGry;
				wyslijLiczby(clientFd, tablica, 1, buffer, tmpBuffer);
				
				// wyslanie informacji o wysokosci i szerokosci planszy
				strcpy(buffer, "03");
				tablica[0] = biezacyPokoj->wysokoscPlanszy;
				tablica[1] = biezacyPokoj->szerokoscPlanszy;
				wyslijLiczby(clientFd, tablica, 2, buffer, tmpBuffer);
				
				// wyslanie informacji o pozostalej liczbie min do odznaczenia
				strcpy(buffer, "04");
				tablica[0] = biezacyPokoj->liczbaMinDoOznaczenia;
				wyslijLiczby(clientFd, tablica, 1, buffer, tmpBuffer);
				
				// wyslanie informacji o zmodyfikowanych polach
				for(row=0; row < biezacyPokoj->wysokoscPlanszy; row++) {
					for(col = 0; col < biezacyPokoj->szerokoscPlanszy; col++) {
						if(biezacyPokoj->stanPlanszy[row][col] == 3) {
							// wyslanie informacji o odkrytym polu
							strcpy(buffer, "06");
							tablica[0] = row;
							tablica[1] = col;
							tablica[2] = biezacyPokoj->plansza[row][col];
							wyslijLiczby(clientFd, tablica, 3, buffer, tmpBuffer);
						} else if(biezacyPokoj->stanPlanszy[row][col] != 0){
							// wyslanie informacji o oznaczonym polu
							strcpy(buffer, "05");
							tablica[0] = row;
							tablica[1] = col;
							tablica[2] = biezacyPokoj->stanPlanszy[row][col];
							wyslijLiczby(clientFd, tablica, 3, buffer, tmpBuffer);
						}
					}
				}
				
				printf("Zakonczono dolaczanie do wybranego pokoju.\n");
				
				
			} else if(kod == 4) {
				// klient prosi o oznaczenie flaga danego pola
				row = strtol(tresc, &tresc, 10);
				col = strtol(tresc, &tresc, 10);
				printf("Trwa oznaczanie flaga pola (%d, %d)...\n", row, col);
				
				if(biezacyPokoj == NULL) {
					printf("Blad - nie mozna zadac oznaczenia flagi, skoro nie gra sie w zadnym z pokojow.\n");
					continue;
				}
				
				if(biezacyPokoj->stanGry != 1) {
					printf("Blad - gra juz sie zakonczyla.\n");
					continue;
				}
				
				if((row < 0) || (row >= biezacyPokoj->wysokoscPlanszy) || (col < 0) || (col >= biezacyPokoj->szerokoscPlanszy)) {
					printf("Blad - wskazane pole nie znajduje sie w zakresie planszy.\n");
					continue;
				}
				
				if(biezacyPokoj->stanPlanszy[row][col] != 0) {
					printf("Blad - flage mozna oznaczyc tylko na nieoznaczonym polu.\n");
					continue;
				}
				
				biezacyPokoj->stanPlanszy[row][col] = 1;
				biezacyPokoj->liczbaMinDoOznaczenia--;
				
				// wyslanie informacji o pozostalej liczbie min do odznaczenia
				strcpy(buffer, "04");
				tablica[0] = biezacyPokoj->liczbaMinDoOznaczenia;
				wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 1, buffer, tmpBuffer);
				
				// wyslanie informacji o oznaczonym polu
				strcpy(buffer, "05");
				tablica[0] = row;
				tablica[1] = col;
				tablica[2] = biezacyPokoj->stanPlanszy[row][col];
				wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 3, buffer, tmpBuffer);
				
				printf("Zakonczono oznaczanie flaga.\n");
				
				
			} else if(kod == 5) {
				// klient prosi o oznaczenie znakiem zapytania danego pola
				row = strtol(tresc, &tresc, 10);
				col = strtol(tresc, &tresc, 10);
				printf("Trwa oznaczanie znakiem zapytania pola (%d, %d)...\n", row, col);
				
				if(biezacyPokoj == NULL) {
					printf("Blad - nie mozna zadac oznaczenia znaku zapytania, skoro nie gra sie w zadnym z pokojow.\n");
					continue;
				}
				
				if(biezacyPokoj->stanGry != 1) {
					printf("Blad - gra juz sie zakonczyla.\n");
					continue;
				}
				
				if((row < 0) || (row >= biezacyPokoj->wysokoscPlanszy) || (col < 0) || (col >= biezacyPokoj->szerokoscPlanszy)) {
					printf("Blad - wskazane pole nie znajduje sie w zakresie planszy.\n");
					continue;
				}
				
				if(biezacyPokoj->stanPlanszy[row][col] != 1) {
					printf("Blad - znak zapytania mozna oznaczyc tylko na polu oznaczonym flaga.\n");
					continue;
				}
				
				biezacyPokoj->stanPlanszy[row][col] = 2;
				biezacyPokoj->liczbaMinDoOznaczenia++;
				
				// wyslanie informacji o pozostalej liczbie min do odznaczenia
				strcpy(buffer, "04");
				tablica[0] = biezacyPokoj->liczbaMinDoOznaczenia;
				wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 1, buffer, tmpBuffer);
				
				// wyslanie informacji o oznaczonym polu
				strcpy(buffer, "05");
				tablica[0] = row;
				tablica[1] = col;
				tablica[2] = biezacyPokoj->stanPlanszy[row][col];
				wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 3, buffer, tmpBuffer);
				
				printf("Zakonczono oznaczanie znakiem zapytania.\n");
				
				
			} else if(kod == 6) {
				// klient prosi o nieoznaczenie danego pola
				row = strtol(tresc, &tresc, 10);
				col = strtol(tresc, &tresc, 10);
				printf("Trwa nieoznaczanie pola (%d, %d)...\n", row, col);
				
				if(biezacyPokoj == NULL) {
					printf("Blad - nie mozna zadac nieoznaczenia pola, skoro nie gra sie w zadnym z pokojow.\n");
					continue;
				}
				
				if(biezacyPokoj->stanGry != 1) {
					printf("Blad - gra juz sie zakonczyla.\n");
					continue;
				}
				
				if((row < 0) || (row >= biezacyPokoj->wysokoscPlanszy) || (col < 0) || (col >= biezacyPokoj->szerokoscPlanszy)) {
					printf("Blad - wskazane pole nie znajduje sie w zakresie planszy.\n");
					continue;
				}
				
				if(biezacyPokoj->stanPlanszy[row][col] != 2) {
					printf("Blad - nieoznaczyc mozna tylko pole oznaczone znakiem zapytania.\n");
					continue;
				}
				
				biezacyPokoj->stanPlanszy[row][col] = 0;
				
				// wyslanie informacji o oznaczonym polu
				strcpy(buffer, "05");
				tablica[0] = row;
				tablica[1] = col;
				tablica[2] = biezacyPokoj->stanPlanszy[row][col];
				wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 3, buffer, tmpBuffer);
				
				printf("Zakonczono nieoznaczanie.\n");
				
				
			} else if(kod == 7) {
				// klient prosi o odkrycie danego pola
				row = strtol(tresc, &tresc, 10);
				col = strtol(tresc, &tresc, 10);
				printf("Trwa odkrywanie pola (%d, %d)...\n", row, col);
				
				if(biezacyPokoj == NULL) {
					printf("Blad - nie mozna zadac nieoznaczenia pola, skoro nie gra sie w zadnym z pokojow.\n");
					continue;
				}
				
				if(biezacyPokoj->stanGry != 1) {
					printf("Blad - gra juz sie zakonczyla.\n");
					continue;
				}
				
				if((row < 0) || (row >= biezacyPokoj->wysokoscPlanszy) || (col < 0) || (col >= biezacyPokoj->szerokoscPlanszy)) {
					printf("Blad - wskazane pole nie znajduje sie w zakresie planszy.\n");
					continue;
				}
				
				if((biezacyPokoj->stanPlanszy[row][col] == 1) || (biezacyPokoj->stanPlanszy[row][col] == 3)) {
					printf("Blad - odkryc mozna tylko pole nieoznaczone lub oznaczone znakiem zapytania.\n");
					continue;
				}
				
				if(biezacyPokoj->plansza[row][col] > 0) {
					printf("Odkryto w polu (%d, %d) liczbe %d\n", row, col, biezacyPokoj->plansza[row][col]);
					biezacyPokoj->stanPlanszy[row][col] = 3;
					biezacyPokoj->liczbaNieodkrytychPol--;
					
					// wyslanie informacji o odkrytym polu z liczba
					strcpy(buffer, "06");
					tablica[0] = row;
					tablica[1] = col;
					tablica[2] = biezacyPokoj->plansza[row][col];
					wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 3, buffer, tmpBuffer);
					
					if(biezacyPokoj->liczbaNieodkrytychPol == biezacyPokoj->liczbaMin) {
						biezacyPokoj->stanGry = 2;
						
						// wysylanie informacji o wygranej grze
						strcpy(buffer, "02");
						tablica[0] = 2;
						wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 1, buffer, tmpBuffer);
					}
				} else if(biezacyPokoj->plansza[row][col] == 0) {
					printf("Odkryto puste pole (%d, %d)\n", row, col);
					
					odkryjPlanszeFloodFill(biezacyPokoj, row, col, tablica, buffer, tmpBuffer);
					
					if(biezacyPokoj->liczbaNieodkrytychPol == biezacyPokoj->liczbaMin) {
						biezacyPokoj->stanGry = 2;
						
						// wysylanie informacji o wygranej grze
						strcpy(buffer, "02");
						tablica[0] = 2;
						wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 1, buffer, tmpBuffer);
					}
				} else {
					printf("Trafiono bombe na polu (%d, %d)\n", row, col);
					biezacyPokoj->stanGry = 0;
					
					// wyslanie informacji o przegranej grze
					strcpy(buffer, "02");
					tablica[0] = 0;
					wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 1, buffer, tmpBuffer);
					
					odkryjBomby(biezacyPokoj, tablica, buffer, tmpBuffer);
				}
				
				printf("Zakonczono odkrywanie pola.\n");
				
				
			} else if(kod == 8) {
				// klient prosi o wyjscie z biezacego pokoju
				printf("Trwa opuszczanie pokoju...\n");
				
				if(biezacyPokoj == NULL) {
					printf("Blad - nie mozna opuscic pokoju, skoro nie gra sie w zadnym z pokojow.\n");
					continue;
				}
				
				i = 0;
				while(biezacyPokoj->gracze[i] != clientFd) {
					i++;
				}
				biezacyPokoj->liczbaGraczy--;
				biezacyPokoj->gracze[i] = biezacyPokoj->gracze[ biezacyPokoj->liczbaGraczy ];
				
				if(biezacyPokoj->liczbaGraczy == 0) {
					std::list<room>::iterator it;
					for(it = pokojeGier.begin(); it != pokojeGier.end(); it++) {
						if(it->id == biezacyPokoj->id) {
							break;
						}
					}
					pokojeGier.erase(it);
				}
				
				biezacyPokoj = NULL;
				
				
				// wysylanie spisu dostepnych pokoi gier
				strcpy(buffer, "01 ");
				printf("Trwa wysylanie listy pokoi...\n");
				
				std::list<room>::iterator it;
				for(it = pokojeGier.begin(); it != pokojeGier.end(); it++) {
					// zapisanie do wiadomosci id pokoju
					itoa(it->id, tmpBuffer, 10);
					strcpy(&buffer[3], tmpBuffer);
					
					// zapisanie do wiadomosci nazwy pokoju
					count = strlen(buffer);
					buffer[count] = ' ';
					strcpy(&buffer[count + 1], it->nazwa);
					
					// zakonczenie wiadomosci znakiem konca lini
					zakonczWiadomoscZnakiemKoncaLini(buffer);
					
					// wyslanie wiadomosci
					count = strlen(buffer);
					//write(clientFd, buffer, count);
					wyslijDane(clientFd, buffer, count);
				}
				
				printf("Zakonczono opuszczanie pokoju i wysylanie listy dostepnych pokoi gier.\n");
				
				
			} else if(kod == 9) {
				// klient prosi o restartowanie planszy
				printf("Trwa restartowanie planszy...\n");
				
				if(biezacyPokoj == NULL) {
					printf("Blad - nie mozna restartowac planszy, skoro nie gra sie w zadnym z pokojow.\n");
					continue;
				}
				
				ustawNowaGre(*biezacyPokoj, biezacyPokoj->wysokoscPlanszy, biezacyPokoj->szerokoscPlanszy, biezacyPokoj->liczbaMin);
				
				// wyslanie polecenia rozpoczecia nowej gry
				strcpy(buffer, "07");
				wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 0, buffer, tmpBuffer);
				
				printf("Zakonczono restartowanie planszy.\n");
				
				
			} else if(kod == 10) {
				// klient prosi o ustawienie nowej gry o nowych parametrach
				row = strtol(tresc, &tresc, 10);
				col = strtol(tresc, &tresc, 10);
				count = strtol(tresc, &tresc, 10);
				printf("Trwa ustawianie nowej gry...\n");
				
				if(biezacyPokoj == NULL) {
					printf("Blad - nie mozna modyfikowac planszy, skoro nie gra sie w zadnym z pokojow.\n");
					continue;
				}
				
				ustawNowaGre(*biezacyPokoj, row, col, count);
				
				// wyslanie informacji o wysokosci i szerokosci planszy
				strcpy(buffer, "03");
				tablica[0] = biezacyPokoj->wysokoscPlanszy;
				tablica[1] = biezacyPokoj->szerokoscPlanszy;
				wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 2, buffer, tmpBuffer);
				
				// wyslanie informacji o pozostalej liczbie min do odznaczenia
				strcpy(buffer, "04");
				tablica[0] = biezacyPokoj->liczbaMinDoOznaczenia;
				wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 1, buffer, tmpBuffer);
				
				// wyslanie polecenia o rozpoczeciu nowej gry
				strcpy(buffer, "07");
				wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 0, buffer, tmpBuffer);
				
				printf("Zakonczono ustawianie nowej gry.\n");
				
				
			} else if(kod == 11) {
				// klient prosi o odkrycie pól dookoła wskazanego pola z liczbą na podstawie oflagowanych już min
				row = strtol(tresc, &tresc, 10);
				col = strtol(tresc, &tresc, 10);
				printf("Trwa odkrywanie pol dookola wskazanego pola z liczba...\n");
				
				if(biezacyPokoj == NULL) {
					printf("Blad - nie mozna odkrywac pol, skoro nie gra sie w zadnym z pokojow.\n");
					continue;
				}
				
				if(biezacyPokoj->stanGry != 1) {
					printf("Blad - gra juz sie zakonczyla.\n");
					continue;
				}
				
				if((row < 0) || (row >= biezacyPokoj->wysokoscPlanszy) || (col < 0) || (col >= biezacyPokoj->szerokoscPlanszy)) {
					printf("Blad - wskazane pole nie znajduje sie w zakresie planszy.\n");
					continue;
				}
				
				if((biezacyPokoj->stanPlanszy[row][col] != 3) || (biezacyPokoj->plansza[row][col] <= 0)) {
					printf("Blad - odkrywac mozna tylko na podstawie odkrytej juz liczby na planszy.\n");
					continue;
				}
				
				count = 0;
				for(i = row - 1; i <= row + 1; i++) {
					for(j = col - 1; j <= col + 1; j++) {
						if((i >= 0) && (i < biezacyPokoj->wysokoscPlanszy) && (j >= 0) && (j < biezacyPokoj->szerokoscPlanszy) && (biezacyPokoj->stanPlanszy[i][j] == 1)) {
							count++;
						}
					}
				}
				
				if(count != biezacyPokoj->plansza[row][col]) {
					printf("Blad - liczba oznaczonych dookola flag nie zgadza sie z liczba w polu (%d, %d).\n", row, col);
					continue;
				}
				
				std::queue<int> kolejkaOdkrytychPustychPol;
				bool bomba = false;
				
				for(i = row - 1; i <= row + 1; i++) {
					for(j = col - 1; j <= col + 1; j++) {
						if((i >= 0) && (i < biezacyPokoj->wysokoscPlanszy) && (j >= 0) && (j < biezacyPokoj->szerokoscPlanszy) && (biezacyPokoj->stanPlanszy[i][j] != 1) && (biezacyPokoj->stanPlanszy[i][j] != 3)) {
							biezacyPokoj->stanPlanszy[i][j] = 3;
							biezacyPokoj->liczbaNieodkrytychPol--;
							
							strcpy(buffer, "06");
							tablica[0] = i;
							tablica[1] = j;
							tablica[2] = biezacyPokoj->plansza[i][j];
							wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 3, buffer, tmpBuffer);
							
							if((biezacyPokoj->plansza[i][j] == -1) && (bomba == false)) {
								printf("Trafiono bombe na polu (%d, %d)\n", i, j);
								bomba = true;
								
								// wysylanie informacji o przegranej grze
								strcpy(buffer, "02");
								tablica[0] = 0;
								wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 1, buffer, tmpBuffer);
							} else if(biezacyPokoj->plansza[i][j] == 0) {
								kolejkaOdkrytychPustychPol.push(i);
								kolejkaOdkrytychPustychPol.push(j);
							}
						}
					}
				}
				
				while(!kolejkaOdkrytychPustychPol.empty()) {
					i = kolejkaOdkrytychPustychPol.front();
					kolejkaOdkrytychPustychPol.pop();
					j = kolejkaOdkrytychPustychPol.front();
					kolejkaOdkrytychPustychPol.pop();
					odkryjPlanszeFloodFill(biezacyPokoj, i, j, tablica, buffer, tmpBuffer);
				}
				
				if(bomba) {
					odkryjBomby(biezacyPokoj, tablica, buffer, tmpBuffer);
				} else if(biezacyPokoj->liczbaNieodkrytychPol == biezacyPokoj->liczbaMin) {
					biezacyPokoj->stanGry = 2;
					
					// wysylanie informacji o wygranej grze
					strcpy(buffer, "02");
					tablica[0] = 2;
					wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 1, buffer, tmpBuffer);
				}
				
				printf("Zakonczono odkrywanie pol dookola wskazanego pola z liczba.\n");
			}
		}
	}
}

int main(int argc, char ** argv){
	
	srand(time(0));
	
	short port = 61596;
	
	// create socket
	servFd = socket(AF_INET, SOCK_STREAM, 0);
	if(servFd == -1) error(1, errno, "Blad podczas socket()");
	
	// graceful ctrl+c exit
	signal(SIGINT, ctrl_c);
	// prevent dead sockets from throwing pipe errors on write
	signal(SIGPIPE, SIG_IGN);
	
	setReuseAddr(servFd);
	
	// bind to any address and port provided in arguments
	sockaddr_in serverAddr{.sin_family=AF_INET, .sin_port=htons(port), .sin_addr={INADDR_ANY}};
	int res = bind(servFd, (sockaddr*) &serverAddr, sizeof(serverAddr));
	if(res) error(1, errno, "Blad podczas bind()");
	
	// enter listening mode
	res = listen(servFd, 1);
	if(res) error(1, errno, "Blad podczas listen()");
	
	while(true){
		// prepare placeholders for client address
		sockaddr_in clientAddr{0};
		socklen_t clientAddrSize = sizeof(clientAddr);
		
		// accept new connection
		auto clientFd = accept(servFd, (sockaddr*) &clientAddr, &clientAddrSize);
		if(clientFd == -1) error(1, errno, "Blad podczas accept()");
		
		// add client to all clients set
		clientFds.insert(clientFd);
		
		// tell who has connected
		printf("\n//****** Nowe polaczenie z klientem: %s:%hu (fd: %d) ******\\\\\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), clientFd);
		
		// przekierowanie odczytywania wiadomosci od nowego klienta do osobnego watku
		std::thread watek(odczytajWiadomosc, clientFd, clientAddr);
		watek.detach();
		
	}
}

void setReuseAddr(int sock){
	const int one = 1;
	int res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if(res) error(1,errno, "Blad podczas setsockopt");
}

void ctrl_c(int){
	for(int clientFd : clientFds)
		close(clientFd);
	close(servFd);
	printf("\nZamykanie serwera\n");
	exit(0);
}

void dodajGraczaDoPokoju(room &pokoj, int clientFd) {
	pokoj.gracze[pokoj.liczbaGraczy] = clientFd;
	pokoj.liczbaGraczy++;
}

void ustawNowaGre(room &pokoj, int wysokoscPlanszy, int szerokoscPlanszy, int liczbaMin) {
	int row, col, i, j;
	
	// zwolnienie pamieci po porzedniej grze
	if(pokoj.liczbaGraczy > 0) {
		for(row = 0; row < pokoj.wysokoscPlanszy; row++) {
			delete[]pokoj.stanPlanszy[row];
		}
		delete[]pokoj.stanPlanszy;
		
		for(row = 0; row < pokoj.wysokoscPlanszy; row++) {
			delete[]pokoj.plansza[row];
		}
		delete[]pokoj.plansza;
	}
	
	if((wysokoscPlanszy <= 1) || (wysokoscPlanszy > 200)) {
		wysokoscPlanszy = 10;
	}
	
	if((szerokoscPlanszy <= 1) || (szerokoscPlanszy > 200)) {
		szerokoscPlanszy = 10;
	}
	
	if(liczbaMin <= 0) {
		liczbaMin = 10;
	}
	
	if(liczbaMin >= wysokoscPlanszy * szerokoscPlanszy) {
		liczbaMin = wysokoscPlanszy * szerokoscPlanszy - 1;
	}
	
	
	pokoj.wysokoscPlanszy = wysokoscPlanszy;
	pokoj.szerokoscPlanszy = szerokoscPlanszy;
	pokoj.liczbaMin = liczbaMin;
	pokoj.liczbaMinDoOznaczenia = liczbaMin;
	pokoj.stanGry = 1;
	pokoj.liczbaNieodkrytychPol = wysokoscPlanszy * szerokoscPlanszy;
	
	pokoj.stanPlanszy = new int*[wysokoscPlanszy];
	for(row = 0; row < wysokoscPlanszy; row++) {
		pokoj.stanPlanszy[row] = new int[szerokoscPlanszy];
		for(col = 0; col < szerokoscPlanszy; col++) {
			pokoj.stanPlanszy[row][col] = 0;
		}
	}
	
	pokoj.plansza = new int*[wysokoscPlanszy];
	for(row = 0; row < wysokoscPlanszy; row++) {
		pokoj.plansza[row] = new int[szerokoscPlanszy];
		for(col = 0; col < szerokoscPlanszy; col++) {
			pokoj.plansza[row][col] = 0;
		}
	}
	
	while(liczbaMin) {
		row = rand() % wysokoscPlanszy;
		col = rand() % szerokoscPlanszy;
		
		if(pokoj.plansza[row][col] != -1) {
			pokoj.plansza[row][col] = -1;
			liczbaMin--;
			
			for(i = row - 1; i <= row + 1; i++) {
				for(j = col - 1; j <= col + 1; j++) {
					if((i >= 0) && (i < wysokoscPlanszy) && (j >= 0) && (j < szerokoscPlanszy) && ((i != row) || (j != col)) && (pokoj.plansza[i][j] != -1)) {
						pokoj.plansza[i][j]++;
					}
				}
			}
		}
	}
}

void zakonczWiadomoscZnakiemKoncaLini(char *buffer) {
	int count;
	
	count = strlen(buffer);
	buffer[count] = '\n';
	buffer[count + 1] = '\0';
}

void itoa(int liczba, char *tekst, int podstawa) {
	std::stack<char> stos;
	int i;
	bool ujemna;
	
	if(liczba == 0) {
		strcpy(tekst, "0");
		return;
	}
	
	if(liczba < 0) {
		liczba *= -1;
		ujemna = true;
	} else {
		ujemna = false;
	}
	
	while(liczba) {
		stos.push('0' + liczba % podstawa);
		liczba /= podstawa;
	}
	
	if(ujemna) {
		tekst[0] = '-';
		i = 1;
	} else {
		i = 0;
	}
	
	while(!stos.empty()) {
		tekst[i] = stos.top();
		stos.pop();
		i++;
	}
	
	tekst[i] = '\0';
}

void wyslijLiczby(int clientFd, int *tablica, int n, char *buffer, char *tmpBuffer) {
	int count, i;
	
	for(i = 0; i < n; i++) {		
		count = strlen(buffer);
		buffer[count] = ' ';
		itoa(tablica[i], tmpBuffer, 10);
		strcpy(&buffer[count + 1], tmpBuffer);
	}
	
	// zakonczenie wiadomosci znakiem konca lini
	zakonczWiadomoscZnakiemKoncaLini(buffer);
	
	// wyslanie wiadomosci
	count = strlen(buffer);
	//write(clientFd, buffer, count);
	wyslijDane(clientFd, buffer, count);
}

void wyslijDoWszystkich(room *biezacyPokoj, char * buffer, int count){
	int i;
	for(i = 0; i < biezacyPokoj->liczbaGraczy; i++) {
		//write(biezacyPokoj->gracze[i], buffer, count);
		wyslijDane(biezacyPokoj->gracze[i], buffer, count);
	}
}

void wyslijLiczbyDoWszystkich(room *biezacyPokoj, int *tablica, int n, char *buffer, char *tmpBuffer) {
	int count, i;
	
	for(i = 0; i < n; i++) {		
		count = strlen(buffer);
		buffer[count] = ' ';
		itoa(tablica[i], tmpBuffer, 10);
		strcpy(&buffer[count + 1], tmpBuffer);
	}
	
	// zakonczenie wiadomosci znakiem konca lini
	zakonczWiadomoscZnakiemKoncaLini(buffer);
	
	// wyslanie wiadomosci
	count = strlen(buffer);
	wyslijDoWszystkich(biezacyPokoj, buffer, count);
}

void odkryjPlanszeFloodFill(room *biezacyPokoj, int row, int col, int *tablica, char *buffer, char *tmpBuffer) {
	std::queue<int> kolejka;
	int i, j;
	
	if(biezacyPokoj->stanPlanszy[row][col] != 3) {
		// wyslanie informacji o odkrytym polu z liczba
		strcpy(buffer, "06");
		tablica[0] = row;
		tablica[1] = col;
		tablica[2] = biezacyPokoj->plansza[row][col];
		wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 3, buffer, tmpBuffer);
		
		biezacyPokoj->stanPlanszy[row][col] = 3;
		biezacyPokoj->liczbaNieodkrytychPol--;
	}
	
	kolejka.push(row);
	kolejka.push(col);
	
	
	while(!kolejka.empty()) {
		row = kolejka.front();
		kolejka.pop();
		col = kolejka.front();
		kolejka.pop();
		
		for(i = row - 1; i <= row + 1; i++) {
			for(j = col - 1; j <= col + 1; j++) {
				if((i >= 0) && (i < biezacyPokoj->wysokoscPlanszy) && (j >= 0) && (j < biezacyPokoj->szerokoscPlanszy) && (biezacyPokoj->stanPlanszy[i][j] != 3)) {
					// wyslanie informacji o odkrytym polu z liczba
					strcpy(buffer, "06");
					tablica[0] = i;
					tablica[1] = j;
					tablica[2] = biezacyPokoj->plansza[i][j];
					wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 3, buffer, tmpBuffer);
					
					biezacyPokoj->stanPlanszy[i][j] = 3;
					biezacyPokoj->liczbaNieodkrytychPol--;
					if(biezacyPokoj->plansza[i][j] == 0) {
						kolejka.push(i);
						kolejka.push(j);
					}
				}
			}
		}
	}
}

void odkryjBomby(room *biezacyPokoj, int *tablica, char *buffer, char *tmpBuffer) {
	int row, col;
	
	//wysylanie informacji o pozostalych polach z minami
	for(row = 0; row < biezacyPokoj->wysokoscPlanszy; row++) {
		for(col = 0; col < biezacyPokoj->szerokoscPlanszy; col++) {
			if((biezacyPokoj->plansza[row][col] == -1) && (biezacyPokoj->stanPlanszy[row][col] != 3)) {
				biezacyPokoj->stanPlanszy[row][col] = 3;
				biezacyPokoj->liczbaNieodkrytychPol--;
				
				strcpy(buffer, "06");
				tablica[0] = row;
				tablica[1] = col;
				tablica[2] = -1;
				wyslijLiczbyDoWszystkich(biezacyPokoj, tablica, 3, buffer, tmpBuffer);
			}
		}
	}
}

void wyslijDane(int fd, char *buffer, int count){
	int ret = write(fd, buffer, count);
	if(ret == -1) error(1, errno, "Blad funkcji write() na kliencie %d", fd);
	if(ret != count) error(0, errno, "Blad funkcji write() na kliencie %d - zapisano mniej danych niz oczekiwano: (%d/%d)", fd, count, ret);
}