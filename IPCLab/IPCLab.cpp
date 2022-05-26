#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include <thread>
#include <future>
#include <iostream>


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define PORT_F "27015"
#define PORT_G "27016"

int getResult(char* serverName, char* address, const char* port, const char* msg)
{
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;
    int res = 0;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(address, port, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    // Send an initial buffer
    iResult = send(ConnectSocket, msg, (int)strlen(msg), 0);
    if (iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    printf("Bytes Sent: %ld\n", iResult);

    // shutdown the connection since no more data will be sent
    iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    // Receive until the peer closes the connection
    do {

        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0)
        {
            printf("Bytes received: %d\n", iResult);

            std::stringstream str(recvbuf);
            str >> res;
        }
        else if (iResult == 0)
            printf("Connection closed\n");
        else
            printf("recv failed with error: %d\n", WSAGetLastError());

    } while (iResult > 0);

    // cleanup
    closesocket(ConnectSocket);
    WSACleanup();

    return res;
}

int f(char* serverName, char* address)
{
    return getResult(serverName, address, PORT_F, "message for F");
}

int g(char* serverName, char* address)
{
    return getResult(serverName, address, PORT_G, "message for F");;
}

std::string askOnStuck()
{
    std::string cmd;
    do
    {
        std::cout << "Process is taking too long. Continue?\n[Y\\N]";
        std::cin >> cmd;
    } while (cmd != "y" && cmd != "Y" && cmd != "n" && cmd != "N");
    return cmd;
}

void printRes(int res)
{
    std::cout << "\nResult: " << res << std::endl;
}

int __cdecl main(int argc, char** argv)
{
    int a = 26052022;
    int* addr = &a;

    // Validate the parameters
    if (argc != 2) {
        printf("usage: %s server-name\n", argv[0]);
        return 1;
    }

    auto futureObjF = std::async(std::launch::async, f, argv[0], argv[1]);
    auto futureObjG = std::async(std::launch::async, g, argv[0], argv[1]);
    std::future_status statusF, statusG;
    auto now = std::chrono::high_resolution_clock::now();

    int fRes = 0, gRes = 0;
    bool gotF = false, gotG = false;

    do
    {
        if (!gotF) statusF = futureObjF.wait_for(std::chrono::milliseconds(0));
        if (!gotG) statusG = futureObjG.wait_for(std::chrono::milliseconds(0));
        auto now2 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> ellapsed = now2 - now;
        if (ellapsed.count() >= 5.0)
        {
            now = std::chrono::high_resolution_clock::now();
            std::string cmd = askOnStuck();
            if (cmd == "n" || cmd == "N") return 0;
        }

        if (!gotF && statusF == std::future_status::ready && (gotF = true) && (fRes = futureObjF.get()) == 0 ||
            !gotG && statusG == std::future_status::ready && (gotG = true) && (gRes = futureObjG.get()) == 0)
        {
            printRes(0);
            exit(0);
        }
    } while (statusF != std::future_status::ready || statusG != std::future_status::ready);

    fRes = !gotF ? futureObjF.get() : fRes;
    gRes = !gotG ? futureObjG.get() : gRes;
    printRes(fRes * gRes);

    std::cin.get();
    return 0;
}