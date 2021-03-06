// opencv_server.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/utils/trace.hpp>

using namespace cv;
//using namespace std;

#pragma comment (lib,"Ws2_32.lib")
//#pragma comment (lib,"Mswsock.lib")
//#pragma comment (lib,"AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "24015"

int main()
{
	WSADATA wsaData;
	int iResult;
	struct addrinfo hints;
	struct addrinfo *result = NULL;
	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;
	const char *server_addr = "127.0.0.1";
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}
	ZeroMemory(&hints, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	iResult = getaddrinfo(server_addr, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}
	iResult=bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);
	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
	ClientSocket = accept(ListenSocket, NULL, NULL);
	if (ClientSocket == INVALID_SOCKET) {
		printf("accept failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
	// No longer need server socket
	closesocket(ListenSocket);//如果只是连接一次就不需要，有两个一个client socket一个listen
	char img_size[2];
	int height;
	int width ;
	Mat frame ;
	int imgSize ;
	int bytes = 0;
	char  *sockData;
	for(int i=0;true;i++){
		if(i==0){
			recv(ClientSocket, img_size,sizeof(img_size), NULL);
		    height =(int)img_size[0];
			width = (int)img_size[1];
			frame = Mat::zeros(height, width, CV_8UC3);
		    imgSize = frame.total()*frame.elemSize();
		}
		else {
			sockData = new char[imgSize];
			bytes = recv(ClientSocket, sockData, imgSize, 0);
			if (bytes == -1) {
				i = 0;
				break;
			}
			int ptr = 0;
			for (int j = 0; j < height; j++) {
				for (int k = 0; k < width; k++) {
					frame.at<cv::Vec3b>(i, j) = cv::Vec3b(sockData[ptr + 0], sockData[ptr + 1], sockData[ptr + 2]);
					ptr = ptr + 3;
				}
			}
			imshow("trans",frame);
		}
		if (waitKey(1) == 27)
			break;
	}
	return 0;
}

