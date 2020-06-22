#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <string>
#include <sstream>
#include <iomanip>
#include <utility>
#include <iostream>

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <queue>
#include <deque>

using namespace std;
using namespace cv;

const char* ARG_VIDEO_PATH = NULL;
const char* ARG_DUMP_DIR = NULL;
bool ARG_HELP, ARG_OCCUPANCY;


struct MaxContainer {
    void push_front(Mat m) {
        if (internal_container.size() < max_size)
            internal_container.push_front(std::move(m));
        else{
        	internal_container.pop_back(); // do something else
        	internal_container.push_front(std::move(m));
        }

    }

    int size(){
    	return internal_container.size();
    }

    std::deque<Mat> get_container() {
    	return internal_container;
    }
private:
    int max_size=5;
    std::deque<Mat> internal_container;
};


void draw_arrow(Mat img, Point pStart, Point pEnd, double len, double alphaDegrees, Scalar lineColor, Scalar startColor)
{    
	const double PI = acos(-1);
	const int lineThickness = 1;
	const int lineType = CV_8U;

	double angle = atan2((double)(pStart.y - pEnd.y), (double)(pStart.x - pEnd.x));  
	line(img, pStart, pEnd, lineColor, lineThickness, lineType);
	img.at<Vec3b>(pStart) = Vec3b(startColor[0], startColor[1], startColor[2]);
	if(len > 0)
	{
		for(int k = 0; k < 2; k++)
		{
			int sign = k == 1 ? 1 : -1;
			Point arrow(pEnd.x + len * cos(angle + sign * PI * alphaDegrees / 180), pEnd.y + len * sin(angle + sign * PI * alphaDegrees / 180));
			line(img, pEnd, arrow, lineColor, lineThickness, lineType);   
		}
	}
}

Mat accumResid(MaxContainer *resid_mats) {
	std::deque<Mat> container = resid_mats->get_container();
	Mat accum = Mat::zeros(container[0].size(), container[0].type());
	double beta = 1.0f / container.size();
	for (int i = 0; i < container.size(); i++) {
		accum += container[i];
	}

	//accum.convertTo(accum, container[0].type(), container.size());

	return accum;
}

void vis_flow(pair<Mat, int> flow, Mat frame, Mat prev, MaxContainer *resid_mats, Mat& lastResid, const char* dumpDir)
{
	Mat flowComponents[3];
	split(flow.first, flowComponents);
	int rows = flowComponents[0].rows;
	int cols = flowComponents[0].cols;

	Mat img = frame.clone();

	Mat resid = Mat::zeros(img.size(), img.type());
	int sz[] = {rows, cols, 2};
	Mat mvMat(3, sz, CV_8U);

	for(int i = 0; i < rows; i++)
	{
		for(int j = 0; j < cols; j++)
		{
			int dx = flowComponents[0].at<int>(i, j);
			int dy = flowComponents[1].at<int>(i, j);
			int occupancy = flowComponents[2].at<int>(i, j);

			mvMat.at<int>(i, j, 0) = dx;
			mvMat.at<int>(i, j, 1) = dy;
			Point start(double(j) / cols * img.cols + img.cols / cols / 2, double(i) / rows * img.rows + img.rows / rows / 2);
			Point end(start.x + dx, start.y + dy);
			Point endBlock(double(j + 1) / cols  * img.cols + img.cols / cols / 2, double(i + 1) / rows * img.rows + img.rows / rows  / 2);
			if (prev.size().width > 0 && start.x + dx < img.cols && start.x + dx >= 0 && start.x >= 0 &&
				start.y + dx < img.cols && start.y + dy >= 0 && start.y >= 0 &&
				endBlock.x < img.cols && endBlock.y < img.rows && endBlock.x + dx < img.cols && endBlock.y + dy < img.rows ) {
				int small_x = min(start.x, endBlock.x);
				int small_y = min(start.y, endBlock.y);
				int big_x = max(start.x, endBlock.x);
				int big_y = max(start.y, endBlock.y);
				//std::cout << start.x << start.y << endBlock.x << endBlock.y << endl;
				Mat n = resid.colRange(start.x,endBlock.x).rowRange(start.y, endBlock.y);
				Mat ref = prev.colRange(start.x + dx,endBlock.x + dx).rowRange(start.y + dy,endBlock.y + dy);
				ref.copyTo(n);
			}

			
			draw_arrow(img, start, end, 2.0, 20.0, CV_RGB(255, 0, 0), (occupancy == 1 || occupancy == 2) ? CV_RGB(0, 255, 0) : CV_RGB(0, 255, 255));
		}
	}
	
	stringstream s;
	stringstream resids;
	stringstream mvName;
	s << dumpDir << "/" << setfill('0') << setw(6) << flow.second << ".png";
	resids << dumpDir << "/" << setfill('0') << setw(6) << flow.second << "_resid.png";
	mvName << dumpDir << "/" << setfill('0') << setw(6) << flow.second << "_mv.mat";
	Mat dst;
	subtract(resid, frame, dst);
	//imwrite(s.str(), frame);
	/*if (resid_mats->size() > 0) {
		Mat accumMat = accumResid(resid_mats);
		imwrite(resids.str(), accumMat);
	}
	dst.copyTo(lastResid);*/
	cv::FileStorage file(mvName.str(), cv::FileStorage::WRITE);
	file << "mv" << mvMat;
	file << "resid" << dst;


}

pair<Mat, int> read_flow()
{
	int rows, cols, frameIndex;
	bool ok = scanf("# pts=%*d frame_index=%d pict_type=%*c output_type=%*s shape=%dx%d origin=%*s\n", &frameIndex, &rows, &cols) == 3;
	int D = ARG_OCCUPANCY ? 3 : 2;
	
	if(!(ok && rows % D == 0))
	{
		fprintf(stderr, "%s\n", "no rows");
		return make_pair(Mat(), -1);
	}
	
	rows /= D;

	Mat_<int> dx(rows, cols), dy(rows, cols), occupancy(rows, cols);
	occupancy = 1;
	Mat flowComponents[] = {dx, dy, occupancy};
	for(int k = 0; k < D; k++)
		for(int i = 0; i < rows; i++)
			for(int j = 0; j < cols; j++)
				assert(scanf("%d ", &flowComponents[k].at<int>(i, j)) == 1);

	Mat flow;
	merge(flowComponents, 3, flow);
	fprintf(stderr, "%d\n", frameIndex);

	return make_pair(flow, frameIndex);
}

void parse_options(int argc, const char* argv[])
{
	for(int i = 1; i < argc; i++)
	{
		if(strcmp(argv[i], "--occupancy") == 0)
			ARG_OCCUPANCY = true;
		else if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
			ARG_HELP = true;
		else if(i == argc - 1)
			ARG_DUMP_DIR = argv[i];
		else
			ARG_VIDEO_PATH = argv[i];
	}
	if(ARG_HELP || ARG_VIDEO_PATH == NULL || ARG_DUMP_DIR == NULL)
	{
		fprintf(stderr, "Usage: cat mpegflow.txt | ./vis [--occupancy] videoPath dumpDir\n  --help and -h will output this help message.\n  dumpDir specifies the directory to save the visualization images\n  --occupancy will expect --occupancy option used for the mpegflow call and will visualize occupancy grid\n");
		exit(1);
	}
}

int main(int argc, const char* argv[])
{
	parse_options(argc, argv);

	pair<Mat, int> flow = read_flow();
	struct MaxContainer resids {};

	VideoCapture in(ARG_VIDEO_PATH);
	Mat frame;
	Mat prev;
	
	assert(in.read(frame));
	for(int opencvFrameIndex = 1; in.read(frame); opencvFrameIndex++)
	{
		//fprintf(stderr, "%d", flow.second);
		if(opencvFrameIndex == flow.second)
		{
			Mat lastResid = Mat::zeros(frame.size(), frame.type());
			vis_flow(flow, frame, prev, &resids, lastResid, ARG_DUMP_DIR);
			resids.push_front(lastResid);
			prev = frame;
			flow = read_flow();
		}
		else {
			fprintf(stderr, "Skipping frame %d.\n", int(opencvFrameIndex));
			//flow = read_flow();
		}

	}
}
