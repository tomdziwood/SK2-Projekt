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
#include <signal.h>
#include <cstring>
#include <time.h>

struct room {
	char nazwa[256];
	int id, liczbaGraczy, gracze[16];
	int **plansza, **stanPlanszy, wysokoscPlanszy, szerokoscPlanszy;
	int liczbaMin, liczbaMinDoOdznaczenia, stanGry;
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

void odczytajWiadomosc(int clientFd, sockaddr_in clientAddr)
{
	long long int kod;
	room tmpPokoj, *biezacyPokoj = NULL;
	char buffer[256], tmpBuffer[256], *tresc;
	int count, i, tablica[10], row, col;
	
	while(true)
	{
		// read a message
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
			printf("removing %d\n", clientFd);
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
				strcpy(tmpPokoj.nazwa, tresc);
				tmpPokoj.id = liczbaGier;
				tmpPokoj.liczbaGraczy = 0;
				
				dodajGraczaDoPokoju(tmpPokoj, clientFd);
				ustawNowaGre(tmpPokoj, 10, 10, 10);
				
				pokojeGier.push_back(tmpPokoj);
				biezacyPokoj = &(pokojeGier.back());
				liczbaGier++;
				
				/*printf("nazwa pokoju przed: |%s|\n", biezacyPokoj->nazwa);
				printf("nazwa pokoju w globalnej liscie przed: |%s|\n", pokojeGier.back().nazwa);
				strcpy(biezacyPokoj->nazwa, "lololo");
				printf("nazwa pokoju po zmianie: |%s|\n", biezacyPokoj->nazwa);
				printf("nazwa pokoju w globalnej liście po: |%s|\n", pokojeGier.back().nazwa);*/
				
				
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
				tablica[0] = biezacyPokoj->liczbaMinDoOdznaczenia;
				wyslijLiczby(clientFd, tablica, 1, buffer, tmpBuffer);
				
				printf("Dodano pokoj.\n");
			} else if(kod == 2) {
				// klient prosi o otrzymanie spisu dostepnych pokoi gier
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
					write(clientFd, buffer, count);
				}
				printf("Zakonczono wysylanie listy pokoi.\n");
			} else if(kod == 3) {
				// klient prosi o dolaczenie go do wskazanego pokoju (o danym id)
				i = strtol(tresc, NULL, 10);
				
				std::list<room>::iterator it;
				for(it = pokojeGier.begin(); it != pokojeGier.end(); it++) {
					if(it->id == i) {
						break;
					}
				}
				
				if(it == pokojeGier.end()) {
					printf("Klient %d prosi o nieistniejacy serwer.\n", clientFd);
					continue;
				}
				
				printf("Trwa wysylanie danych wybranego pokoju do klienta...\n");
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
				tablica[0] = biezacyPokoj->liczbaMinDoOdznaczenia;
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
				
				printf("Wysylanie danych zakonczone.\n");
			}

			// broadcast the message
			sendToAllBut(clientFd, buffer, count);
		}
	}
}

int main(int argc, char ** argv){
	
	srand(time(0));
	
	short port = 61596;
	
	// create socket
	servFd = socket(AF_INET, SOCK_STREAM, 0);
	if(servFd == -1) error(1, errno, "socket failed");
	
	// graceful ctrl+c exit
	signal(SIGINT, ctrl_c);
	// prevent dead sockets from throwing pipe errors on write
	signal(SIGPIPE, SIG_IGN);
	
	setReuseAddr(servFd);
	
	// bind to any address and port provided in arguments
	sockaddr_in serverAddr{.sin_family=AF_INET, .sin_port=htons(port), .sin_addr={INADDR_ANY}};
	int res = bind(servFd, (sockaddr*) &serverAddr, sizeof(serverAddr));
	if(res) error(1, errno, "bind failed");
	
	// enter listening mode
	res = listen(servFd, 1);
	if(res) error(1, errno, "listen failed");
	
	while(true){
		// prepare placeholders for client address
		sockaddr_in clientAddr{0};
		socklen_t clientAddrSize = sizeof(clientAddr);
		
		// accept new connection
		auto clientFd = accept(servFd, (sockaddr*) &clientAddr, &clientAddrSize);
		if(clientFd == -1) error(1, errno, "accept failed");
		
		// add client to all clients set
		clientFds.insert(clientFd);
		
		// tell who has connected
		printf("new connection from: %s:%hu (fd: %d)\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), clientFd);
		
		// 
		std::thread watek(odczytajWiadomosc, clientFd, clientAddr);
		watek.detach();
		
	}
}

void setReuseAddr(int sock){
	const int one = 1;
	int res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if(res) error(1,errno, "setsockopt failed");
}

void ctrl_c(int){
	for(int clientFd : clientFds)
		close(clientFd);
	close(servFd);
	printf("Closing server\n");
	exit(0);
}

void sendToAllBut(int fd, char * buffer, int count){
	int res;
	decltype(clientFds) bad;
	for(int clientFd : clientFds){
		if(clientFd == fd) continue;
		res = write(clientFd, buffer, count);
		if(res!=count)
			bad.insert(clientFd);
	}
	for(int clientFd : bad){
		printf("removing in sendToAllBut %d\n", clientFd);
		clientFds.erase(clientFd);
		close(clientFd);
	}
}

void dodajGraczaDoPokoju(room &pokoj, int clientFd) {
	pokoj.gracze[pokoj.liczbaGraczy] = clientFd;
	pokoj.liczbaGraczy++;
}

void ustawNowaGre(room &pokoj, int wysokoscPlanszy, int szerokoscPlanszy, int liczbaMin) {
	int row, col, i, j;
	
	if(liczbaMin >= wysokoscPlanszy * szerokoscPlanszy) {
		liczbaMin = wysokoscPlanszy * szerokoscPlanszy - 1;
	}
	
	pokoj.wysokoscPlanszy = wysokoscPlanszy;
	pokoj.szerokoscPlanszy = szerokoscPlanszy;
	pokoj.liczbaMin = liczbaMin;
	pokoj.liczbaMinDoOdznaczenia = liczbaMin;
	pokoj.stanGry = 1;
	
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
	
	if(liczba == 0) {
		strcpy(tekst, "0");
		return;
	}
	
	while(liczba) {
		stos.push('0' + liczba % podstawa);
		liczba /= podstawa;
	}
	
	i = 0;
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
	write(clientFd, buffer, count);
}