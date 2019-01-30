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

ssize_t readData(int fd, char * buffer, ssize_t buffsize){
	auto ret = read(fd, buffer, buffsize);
	if(ret==-1) error(1,errno, "read failed on descriptor %d", fd);
	return ret;
}

void writeData(int fd, char * buffer, ssize_t count){
	auto ret = write(fd, buffer, count);
	if(ret==-1) error(1, errno, "write failed on descriptor %d", fd);
	if(ret!=count) error(0, errno, "wrote less than requested to descriptor %d (%ld/%ld)", fd, count, ret);
}

void czytajZSerwera(int sock)
{
	int buffsize = 255, received;
	char buffer[buffsize], tmpBuffer[buffsize];
	int poczatek, koniec, count = 0;
	
	// read from socket, write to stdout
    while(true)
    {
        received = readData(sock, buffer, buffsize);
		buffer[received] = '\0';
		printf("\n//-------------------- ");
		printf("\n//---- Nowy komunikat o dlugosci %d ----\\\\\n", received);
		printf("|%s|\n", buffer);
        //writeData(1, buffer1, received1);
		
		poczatek = 0;
		koniec = 0;
		printf("Wydzielone wiadomosci:\n");
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
			}
		}
		
    }
}

void wysylajDoSerwera(int sock)
{
	// read from stdin, write to socket
    while(true)
    {
        ssize_t bufsize2 = 255, received2;
        char buffer2[bufsize2];
        received2 = readData(0, buffer2, bufsize2);
        writeData(sock, buffer2, received2);
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
