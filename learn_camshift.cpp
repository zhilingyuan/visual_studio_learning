#include "stdafx.h"
#include <opencv2/core/utility.hpp>
#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include "opencv2/highgui.hpp"

#include <iostream>
#include <ctype.h>

using namespace cv;
using namespace std;

Mat image;

bool backprojMode = false;
bool selectObject = false;
int trackObject = 0;
bool showHist = true;
Point origin;
Rect selection;
int vmin = 10, vmax = 256, smin = 30;

// User draws box around object to track. This triggers CAMShift to start tracking
static void onMouse(int event, int x, int y, int, void*)
{
	if (selectObject)
	{
		selection.x = MIN(x, origin.x);
		selection.y = MIN(y, origin.y);
		selection.width = std::abs(x - origin.x);
		selection.height = std::abs(y - origin.y);
		
		selection &= Rect(0, 0, image.cols, image.rows);//交际 | 并集
	}

	switch (event)
	{
	case EVENT_LBUTTONDOWN:
		origin = Point(x, y);
		selection = Rect(x, y, 0, 0);
		selectObject = true;
		break;
	case EVENT_LBUTTONUP:
		selectObject = false;
		if (selection.width > 0 && selection.height > 0)
			trackObject = -1;   // Set up CAMShift properties in main() loop
		break;
	}
}

string hot_keys =
"\n\nHot keys: \n"
"\tESC - quit the program\n"
"\tc - stop the tracking\n"
"\tb - switch to/from backprojection view\n"
"\th - show/hide object histogram\n"
"\tp - pause video\n"
"To initialize tracking, select the object with mouse\n";

static void help()
{
	cout << "\nThis is a demo that shows mean-shift based tracking\n"
		"You select a color objects such as your face and it tracks it.\n"
		"This reads from video camera (0 by default, or the camera number the user enters\n"
		"Usage: \n"
		"   ./camshiftdemo [camera number]\n";
	cout << hot_keys;
}

const char* keys =
{
	"{help h | | show help message}{@camera_number| 0 | camera number}"
};

int main(int argc, const char** argv)
{
	VideoCapture cap;
	Rect trackWindow;
	int hsize = 16;
	float hranges[] = { 0,180 };
	const float* phranges = hranges;
	CommandLineParser parser(argc, argv, keys);
	if (parser.has("help"))
	{
		help();
		return 0;
	}
	int camNum = parser.get<int>(0);
	cap.open(camNum);

	if (!cap.isOpened())
	{
		help();
		cout << "***Could not initialize capturing...***\n";
		cout << "Current parameter's value: \n";
		parser.printMessage();
		return -1;
	}
	cout << hot_keys;
	namedWindow("Histogram", 0);
	namedWindow("CamShift Demo", 0);
	setMouseCallback("CamShift Demo", onMouse, 0);
	createTrackbar("Vmin", "CamShift Demo", &vmin, 256, 0);
	createTrackbar("Vmax", "CamShift Demo", &vmax, 256, 0);
	createTrackbar("Smin", "CamShift Demo", &smin, 256, 0);

	Mat frame, hsv, hue, mask, hist, histimg = Mat::zeros(200, 320, CV_8UC3), backproj;//赋值0：n—2 =1
	bool paused = false;															   //n-1=0

	for (;;)
	{
		if (!paused)
		{
			cap >> frame;
			if (frame.empty())
				break;
		}

		frame.copyTo(image);

		if (!paused)
		{
			cvtColor(image, hsv, COLOR_BGR2HSV);

			if (trackObject)
			{
				int _vmin = vmin, _vmax = vmax;

				inRange(hsv, Scalar(0, smin, MIN(_vmin, _vmax)),
					Scalar(180, 256, MAX(_vmin, _vmax)), mask);//（hsv,scalar（min，min，min）scalar（max，max，max）,dst）
			
				int ch[] = { 0, 0 };
				hue.create(hsv.size(), hsv.depth());
				mixChannels(&hsv, 1, &hue, 1, ch, 1);
				/*
				ch 解释：
				第一个输入矩阵的通道标记范围为：0 ~ src[0].channels()-1，
				第二个输入矩阵的通道标记范围为：src[0].channels() ~ src[0].channels()+src[1].channels()-1,以此类推；
				其次输出矩阵也用同样的规则标记，第一个输出矩阵的通道标记范围为：0 ~ dst[0].channels()-1，
				第二个输入矩阵的通道标记范围为：dst[0].channels()~ dst[0].channels()+dst[1].channels()-1,以此类推；
				最后，数组fromTo的第一个元素即fromTo[0]应该填入输入矩阵的某个通道标记，而fromTo的第二个元素即fromTo[1]应该填入输出矩阵的某个通道标记，
				这样函数就会把输入矩阵的fromTo[0]通道里面的数据复制给输出矩阵的fromTo[1]通道。fromTo后面的元素也是这个道理，
				总之就是一个输入矩阵的通道标记后面必须跟着个输出矩阵的通道标记。
				*/
				if (trackObject < 0)
				{
					// Object has been selected by user, set up CAMShift search properties once
					Mat roi(hue, selection), maskroi(mask, selection);//实例化一部分区域
					calcHist(&roi, 1, 0, maskroi, hist, 1, &hsize, &phranges);//
					normalize(hist, hist, 0, 255, NORM_MINMAX);
					
					trackWindow = selection;
					trackObject = 1; // Don't set up again, unless user selects new ROI

					histimg = Scalar::all(0);
					int binW = histimg.cols / hsize;
					Mat buf(1, hsize, CV_8UC3);
					for (int i = 0; i < hsize; i++)
						buf.at<Vec3b>(i) = Vec3b(saturate_cast<uchar>(i*180. / hsize), 255, 255); //saturate 防止溢出；
					cvtColor(buf, buf, COLOR_HSV2BGR); //颜色buf
					//imshow("buf",buf);
					for (int i = 0; i < hsize; i++)
					{
						int val = saturate_cast<int>(hist.at<float>(i)*histimg.rows / 255);
						rectangle(histimg, Point(i*binW, histimg.rows),
							Point((i + 1)*binW, histimg.rows - val),
							Scalar(buf.at<Vec3b>(i)), -1, 8);//histimg 绘图
					}
				}

				// Perform CAMShift
				calcBackProject(&hue, 1, 0, hist, backproj, &phranges);
				/*   图像的反向投影图是用输入图像的某一位置上像素值（多维或灰度）对应在直方图的一个bin上的值来代替该像素值，
				所以得到的反向投影图是单通的。用统计学术语，输出图像象素点的值是观测数组在某个分布（直方图）下的概率。


				还是以例子说明
				(1)例如灰度图像如下
				Image=

				0 1 2 3

				4 5 6 7

				8 9 10 11

				8 9 14 15

				(2)该灰度图的直方图为（bin指定的区间为[0,3)，[4,7)，[8,11)，[12,16)）
				Histogram=

				4 4 6 2
				(3)反向投影图

				Back_Projection=

				4 4 4 4

				4 4 4 4

				6 6 6 6

				6 6 2 2*/
				backproj &= mask;
				RotatedRect trackBox = CamShift(backproj, trackWindow,
					TermCriteria(TermCriteria::EPS | TermCriteria::COUNT, 10, 1));
				if (trackWindow.area() <= 1)
				{
					int cols = backproj.cols, rows = backproj.rows, r = (MIN(cols, rows) + 5) / 6;
					trackWindow = Rect(trackWindow.x - r, trackWindow.y - r,
						trackWindow.x + r, trackWindow.y + r) &
						Rect(0, 0, cols, rows);
				}

				if (backprojMode)
					cvtColor(backproj, image, COLOR_GRAY2BGR);
				ellipse(image, trackBox, Scalar(0, 0, 255), 3, LINE_AA);
			}
		}
		else if (trackObject < 0)
			paused = false;

		if (selectObject && selection.width > 0 && selection.height > 0)
		{
			Mat roi(image, selection);
			bitwise_not(roi, roi);
		}

		imshow("CamShift Demo", image);
		imshow("Histogram", histimg);

		char c = (char)waitKey(10);
		if (c == 27)
			break;
		switch (c)
		{
		case 'b':
			backprojMode = !backprojMode;
			break;
		case 'c':
			trackObject = 0;
			histimg = Scalar::all(0);
			break;
		case 'h':
			showHist = !showHist;
			if (!showHist)
				destroyWindow("Histogram");
			else
				namedWindow("Histogram", 1);
			break;
		case 'p':
			paused = !paused;
			break;
		default:
			;
		}
	}

	return 0;
}
