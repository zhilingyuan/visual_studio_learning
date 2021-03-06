// property_opencv.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#define WIN32_LEAN_AND_MEAN
/* OpenCV Application Tracing support demo. */
#include <iostream>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/utils/trace.hpp>

using namespace cv;
using namespace std;

#pragma comment (lib,"Ws2_32.lib")
#pragma comment (lib,"Mswsock.lib")
#pragma comment (lib,"AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

static Mat process_frame(const cv::Mat& frame)
{
	CV_TRACE_FUNCTION(); // OpenCV Trace macro for function

	imshow("Live", frame);
	
	Mat gray, processed;
	cv::cvtColor(frame, gray, COLOR_BGR2GRAY);
	Canny(gray, processed, 32, 64, 3);
	imshow("Processed", processed);
	return processed;

}


int main(int argc, char** argv)
{	//trans initialization
	WSADATA wsaData;
	SOCKET ConnectSocket = INVALID_SOCKET;
	int iResult;
	
	//sockaddr serAddr;
	const char *server_addr = "192.168.1.108";
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error code : %d", iResult);
		return 1;
	}
	
	struct addrinfo hints;
	struct addrinfo *result=NULL,*ptr=NULL;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	iResult = getaddrinfo("192.168.1.108", DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error code : %d", iResult);
		WSACleanup();
		return 1;
	}
	//ptr=result; 没有bind 只是connect 即server ip and port
	ConnectSocket = socket(AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP);
	if(ConnectSocket == INVALID_SOCKET){
		printf("socket failed with error :%ld\n", WSAGetLastError());
		WSACleanup();
		return 1;}
	
	iResult=connect(ConnectSocket, result->ai_addr, result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		closesocket(ConnectSocket);
		printf("socket error and closed %d ", WSAGetLastError());
		waitKey();
		return 1;
	}

	freeaddrinfo(result);

	if (ConnectSocket == INVALID_SOCKET) {
		printf("Unable to connect to server\n");
		WSACleanup();
		return 1;
	}

	CV_TRACE_FUNCTION();
	
	cv::CommandLineParser parser(argc, argv,
		"{help h ? |     | help message}"
		"{n        | -1 | number of frames to process }"
		"{@video   | 0   | video filename or cameraID }"
	);
	if (parser.has("help"))
	{
		parser.printMessage();
		return 0;
	}

	VideoCapture capture;
	std::string video = parser.get<string>("@video");
	if (video.size() == 1 && isdigit(video[0]))
		capture.open(parser.get<int>("@video"));
	else
		capture.open(video);
	int nframes = 0;
	if (capture.isOpened())
	{
		nframes = (int)capture.get(CAP_PROP_FRAME_COUNT);
		cout << "Video " << video <<
			": width=" << capture.get(CAP_PROP_FRAME_WIDTH) <<
			", height=" << capture.get(CAP_PROP_FRAME_HEIGHT) <<
			", nframes=" << nframes << endl;
	}
	else
	{
		cout << "Could not initialize video capturing...\n";
		return -1;
	}

	int N = parser.get<int>("n");
	if (nframes > 0 && N > nframes)
		N = nframes;

	cout << "Start processing..." << endl
		<< "Press ESC key to terminate" << endl;

	Mat frame;
	Mat processed_frame;
	//初始化frame 参数
	int initial=0;
	int img_size[2];
	//
	for (int i = 0; N > 0 ? (i < N) : true; i++)
	{
		CV_TRACE_REGION("FRAME"); // OpenCV Trace macro for named "scope" region
		{
			CV_TRACE_REGION("read");
			capture.read(frame);
			
			if (frame.empty())
			{
				cerr << "Can't capture frame: " << i << std::endl;
				i = initial;
				break;
			}

			if (i == 0) {
				img_size[0] = frame.rows;
				img_size[1] = frame.cols;
				send(ConnectSocket, (char *)img_size, sizeof(img_size), 0);
			}
			else
			{
				// OpenCV Trace macro for NEXT named region in the same C++ scope
				// Previous "read" region will be marked complete on this line.
				// Use this to eliminate unnecessary curly braces.
				CV_TRACE_REGION_NEXT("process");
				processed_frame = process_frame(frame);
				frame.rows;
				int imgSize = frame.total()*frame.elemSize();
				//const char * trans_data=(char *)frame.data;
				//printf(trans_data);
				send(ConnectSocket, (const char *)frame.data, imgSize, 0);


				CV_TRACE_REGION_NEXT("delay");
			}
			if (waitKey(1) == 27/*ESC*/)
				break;
		}
	}
	closesocket(ConnectSocket);
	return 0;
}


/*environment variable
PATH
C:\opencv\build\x64\vc15\bin


include
C:\opencv\build\include;C:\opencv\build\include\opencv;C:\opencv\build\include\opencv2;

lib
C:\opencv\build\x64\vc15\lib;



debug
opencv_world341d.lib

release
opencv_world341.lib
*/

