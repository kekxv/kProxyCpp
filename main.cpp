#ifdef WIN32

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <mstcpip.h>
#include <cstdio>

#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include<unistd.h>
#endif

#include <iostream>
#include <kHttpd.h>
#include <kWebSocketClient.h>
#include <base64.h>
#include <CJsonObject.h>
#include <kHttpdClient.h>
#include <getopt.h>
#include <string>

using namespace std;

#ifdef ENABLE_OPENCV

#include <opencv2/opencv.hpp>

using namespace cv;

typedef struct {
    Point2f left_top;
    Point2f left_bottom;
    Point2f right_top;
    Point2f right_bottom;
} four_corners_t;

void OptimizeSeam(Mat &img1, Mat &trans, Mat &dst,
                  four_corners_t &corners);


void CalcCorners(const Mat &H, const Mat &src,
                 four_corners_t &corners) {
    double v2[] = {0, 0, 1};//左上角
    double v1[3];//变换后的坐标值
    Mat V2 = Mat(3, 1, CV_64FC1, v2);  //列向量
    Mat V1 = Mat(3, 1, CV_64FC1, v1);  //列向量

    V1 = H * V2;
    //左上角(0,0,1)
//    cout << "V2: " << V2 << endl;
//    cout << "V1: " << V1 << endl;
    corners.left_top.x = v1[0] / v1[2];
    corners.left_top.y = v1[1] / v1[2];

    //左下角(0,src.rows,1)
    v2[0] = 0;
    v2[1] = src.rows;
    v2[2] = 1;
    V2 = Mat(3, 1, CV_64FC1, v2);  //列向量
    V1 = Mat(3, 1, CV_64FC1, v1);  //列向量
    V1 = H * V2;
    corners.left_bottom.x = v1[0] / v1[2];
    corners.left_bottom.y = v1[1] / v1[2];

    //右上角(src.cols,0,1)
    v2[0] = src.cols;
    v2[1] = 0;
    v2[2] = 1;
    V2 = Mat(3, 1, CV_64FC1, v2);  //列向量
    V1 = Mat(3, 1, CV_64FC1, v1);  //列向量
    V1 = H * V2;
    corners.right_top.x = v1[0] / v1[2];
    corners.right_top.y = v1[1] / v1[2];

    //右下角(src.cols,src.rows,1)
    v2[0] = src.cols;
    v2[1] = src.rows;
    v2[2] = 1;
    V2 = Mat(3, 1, CV_64FC1, v2);  //列向量
    V1 = Mat(3, 1, CV_64FC1, v1);  //列向量
    V1 = H * V2;
    corners.right_bottom.x = v1[0] / v1[2];
    corners.right_bottom.y = v1[1] / v1[2];

}

int MergePhoto(Mat image01, Mat image02, Mat &outMat) {
    if (image01.empty() || image02.empty())return -1;
    four_corners_t corners;
    //灰度图转换
    Mat image1, image2;
    cvtColor(image01, image1, COLOR_BGR2GRAY);
    cvtColor(image02, image2, COLOR_BGR2GRAY);

    //提取特征点
    auto surfDetector = ORB::create(3000);
    vector<KeyPoint> keyPoint1, keyPoint2;
    surfDetector->detect(image1, keyPoint1);
    surfDetector->detect(image2, keyPoint2);

    //特征点描述，为下边的特征点匹配做准备
//    DescriptorExtractor  SurfDescriptor;
    Mat imageDesc1, imageDesc2;
    surfDetector->compute(image1, keyPoint1, imageDesc1);
    surfDetector->compute(image2, keyPoint2, imageDesc2);
    if (imageDesc1.empty() || imageDesc2.empty())return -1;

    flann::Index flannIndex(imageDesc1, flann::LshIndexParams(12, 20, 2), cvflann::FLANN_DIST_HAMMING);

    vector<DMatch> GoodMatchePoints;

    Mat macthIndex(imageDesc2.rows, 2, CV_32SC1), matchDistance(imageDesc2.rows, 2, CV_32FC1);
    flannIndex.knnSearch(imageDesc2, macthIndex, matchDistance, 2, flann::SearchParams());

    // Lowe's algorithm,获取优秀匹配点
    for (int i = 0; i < matchDistance.rows; i++) {
        if (matchDistance.at<float>(i, 0) < 0.4 * matchDistance.at<float>(i, 1)) {
            DMatch dmatches(i, macthIndex.at<int>(i, 0), matchDistance.at<float>(i, 0));
            GoodMatchePoints.push_back(dmatches);
        }
    }

    // Mat first_match;
    // drawMatches(image02, keyPoint2, image01, keyPoint1, GoodMatchePoints, first_match);
    // imshow("first_match ", first_match);

    vector<Point2f> imagePoints1, imagePoints2;

    for (int i = 0; i < GoodMatchePoints.size(); i++) {
        imagePoints2.push_back(keyPoint2[GoodMatchePoints[i].queryIdx].pt);
        imagePoints1.push_back(keyPoint1[GoodMatchePoints[i].trainIdx].pt);
    }

    if (imagePoints1.empty() || imagePoints2.empty())return -1;

    //获取图像1到图像2的投影映射矩阵 尺寸为3*3
    Mat homo = findHomography(imagePoints1, imagePoints2, RANSAC);
    ////也可以使用getPerspectiveTransform方法获得透视变换矩阵，不过要求只能有4个点，效果稍差
    //Mat   homo=getPerspectiveTransform(imagePoints1,imagePoints2);
    // cout << "变换矩阵为：\n" << homo << endl << endl; //输出映射矩阵

    //计算配准图的四个顶点坐标
    CalcCorners(homo, image01, corners);
    // cout << "left_top:" << corners.left_top << endl;
    // cout << "left_bottom:" << corners.left_bottom << endl;
    // cout << "right_top:" << corners.right_top << endl;
    // cout << "right_bottom:" << corners.right_bottom << endl;

    //图像配准
    Mat imageTransform1, imageTransform2;
    warpPerspective(image01, imageTransform1, homo,
                    Size(MAX(corners.right_top.x, corners.right_bottom.x), image02.rows));
    //warpPerspective(image01, imageTransform2, adjustMat*homo, Size(image02.cols*1.3, image02.rows*1.8));
    // imshow("直接经过透视矩阵变换", imageTransform1);
    // imwrite("trans1.jpg", imageTransform1);


    //创建拼接后的图,需提前计算图的大小
    int dst_width = imageTransform1.cols;  //取最右点的长度为拼接图的长度
    int dst_height = image02.rows;

    Mat dst(dst_height, dst_width, CV_8UC3);
    dst.setTo(0);

    imageTransform1.copyTo(dst(Rect(0, 0, imageTransform1.cols, imageTransform1.rows)));
    if (image02.cols > dst.cols || image02.rows > dst.rows)return -2;
    image02.copyTo(dst(Rect(0, 0, image02.cols, image02.rows)));

    // imshow("b_dst", dst);


    OptimizeSeam(image02, imageTransform1, dst, corners);


    // imshow("dst", dst);
    // imwrite("dst.jpg", dst);

    // waitKey();
    outMat = dst;
    return 0;
}


//优化两图的连接处，使得拼接自然
void OptimizeSeam(Mat &img1, Mat &trans, Mat &dst,
                  four_corners_t &corners) {
    int start = MIN(corners.left_top.x, corners.left_bottom.x);//开始位置，即重叠区域的左边界

    double processWidth = img1.cols - start;//重叠区域的宽度
    int rows = dst.rows;
    int cols = img1.cols; //注意，是列数*通道数
    double alpha = 1;//img1中像素的权重
    for (int i = 0; i < rows; i++) {
        auto *p = img1.ptr<uchar>(i);  //获取第i行的首地址
        auto *t = trans.ptr<uchar>(i);
        auto *d = dst.ptr<uchar>(i);
        for (int j = start; j < cols; j++) {
            //如果遇到图像trans中无像素的黑点，则完全拷贝img1中的数据
            if (t[j * 3] == 0 && t[j * 3 + 1] == 0 && t[j * 3 + 2] == 0) {
                alpha = 1;
            } else {
                //img1中像素的权重，与当前处理点距重叠区域左边界的距离成正比，实验证明，这种方法确实好
                alpha = (processWidth - (j - start)) / processWidth;
            }

            d[j * 3] = p[j * 3] * alpha + t[j * 3] * (1 - alpha);
            d[j * 3 + 1] = p[j * 3 + 1] * alpha + t[j * 3 + 1] * (1 - alpha);
            d[j * 3 + 2] = p[j * 3 + 2] * alpha + t[j * 3 + 2] * (1 - alpha);

        }
    }

}

#endif

void show_help() {
    printf(
            "help (http://kekxv.com)\n\n"
            "\t -d\t后台运行 \n"
            "\t -w\t开启 WebSocket \n"
            "\t -l\t设置监听地址 默认 0.0.0.0\n"
            "\t -p\t设置端口号 默认8080\n"
            "\t -P\t开启PHP，并设置 PHP 端口号\n"
            "\t -n\t线程数量，默认 20，WebSocket 模式建议设置大一点\n"
            "\t -L\t静态文件路径 \n"
            "\t -h\t帮助信息 \n"
            "\t -?\t帮助信息 \n"
    );
}

int main(int argc, char **argv) {
    bool isWebSocket = false;
    bool httpd_option_daemon = false;
    string ip = "0.0.0.0";
    string php_ip;
    unsigned short port = 8080;
    unsigned short php_port = 9000;
    string web_root;
    unsigned int thread_num = 20;

    int opt;
    const char *string = "wdp:P:n:L:l:h?";
    while ((opt = getopt(argc, argv, string)) != -1) {
        switch (opt) {
            case 'w':
                isWebSocket = true;
                break;
            case 'd':
                httpd_option_daemon = true;
                break;
            case 'p':
                port = (int) strtol((const char *) optarg, nullptr, 10);
                break;
            case 'P':
                php_port = (int) strtol((const char *) optarg, nullptr, 10);
                php_ip = "127.0.0.1";
                break;
            case 'n':
                thread_num = (int) strtol((const char *) optarg, nullptr, 10);
                if (thread_num > 200)thread_num = 200;
                break;
            case 'L':
                web_root = optarg;
                break;
            case 'l':
                ip = optarg;
                break;
            default:
                show_help();
                exit(EXIT_SUCCESS);
        }
    }
#ifdef WIN32
#else
    //判断是否设置了-d，以daemon运行
    if (httpd_option_daemon) {
        pid_t pid;
        pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        if (pid > 0) {
            //生成子进程成功，退出父进程
            exit(EXIT_SUCCESS);
        }
    }
#endif

    kHttpd::Init();


    kHttpd kProxy(web_root.c_str(), thread_num);
    kProxy.isWebSocket = isWebSocket;
    if (!php_ip.empty() && php_port > 0) {
        kProxy.init_php(php_ip.c_str(), php_port);
    }

    kProxy.set_gencb([](void *kClient, const std::vector<unsigned char> &data, const std::string &url_path,
                        const std::string &method,
                        const std::string &host, int type, void *arg) -> int {
        return -1;
        // std::cout << "回调调用" << std::endl;
        // return ((kWebSocketClient *) kClient)->send(data, type) >= 0;
    });

#ifdef ENABLE_OPENCV
    kProxy.set_cb("POST", "/MergePhoto",
                  [](void *kClient, const std::vector<unsigned char> &data, const std::string &url_path,
                     const std::string &method, int type, void *arg) -> int {
                      if (type == -1) {
                          auto self = (kHttpdClient *) kClient;
                          std::string contentType = self->header["content-type"];
                          transform(contentType.begin(), contentType.end(), contentType.begin(), ::tolower);
                          if (contentType.find("json") == string::npos)return -1;
                          if (data.empty())return -1;
                          try {
                              self->ContentType = self->header["content-type"];
                              CJsonObject cJsonObject((char *) data.data());
                              CJsonObject retData;
                              auto left = cJsonObject["left"];
                              auto right = cJsonObject["right"];
                              if (left.IsEmpty() || right.IsEmpty()) {
                                  throw std::exception();
                                  return 0;
                              }
                              auto left_str = left.toString();
                              auto right_str = right.toString();
                              if (left_str.empty() || right_str.empty()) {
                                  throw std::exception();
                                  return 0;
                              }
                              auto left_index = left_str.find(",");
                              if (left_index != string::npos) {
                                  left_str = left_str.substr(left_index + 1);
                              }
                              auto right_index = right_str.find(",");
                              if (right_index != string::npos) {
                                  right_str = right_str.substr(left_index + 1);
                              }

                              auto left_bin = base64_decode(left_str);
                              auto right_bin = base64_decode(right_str);

                              Mat image01 = imdecode(right_bin, 1);    //右图
                              Mat image02 = imdecode(left_bin, 1);    //左图

                              Mat outMat;
                              int ret = MergePhoto(image01, image02, outMat);
                              if (ret == 0 && !outMat.empty()) {
                                  vector<unsigned char> photo;
                                  vector<int> compression_params;
                                  compression_params.push_back(IMWRITE_JPEG_QUALITY);
                                  compression_params.push_back(85);
                                  imencode(".jpg", outMat, photo, compression_params);
                                  retData.Add("code", 0);
                                  retData.Add("message", "合并成功");
                                  retData.Add("photo",
                                              "data:image/jpg;base64," + base64_encode(photo.data(), photo.size()));

                              } else {
                                  retData.Add("code", -1);
                                  retData.Add("message", "合并失败");
                              }
                              auto ret_str = retData.ToString();
                              self->ResponseContent.insert(self->ResponseContent.end(), ret_str.begin(), ret_str.end());
                              return 0;
                          } catch (std::exception &e) {
                              CJsonObject retData;
                              retData.Add("code", -1);
                              retData.Add("message", "参数错误");
                              auto ret_str = retData.ToString();
                              self->ResponseContent.insert(self->ResponseContent.end(), ret_str.begin(), ret_str.end());
                              return 0;
                          }
                      } else {
                          return -1;
                      }
                  });
#endif
    kProxy.set_cb([](void *kClient, const std::vector<unsigned char> &data, const std::string &url_path,
                     const std::string &method, int type, void *arg) -> int {
        //        std::cout << "回调调用:" << url_path << " " << method << " " << ((kWebSocketClient *) kClient)->header["host"]
        //                  << std::endl;

        if (type == 1) {
            /*
            return ((kWebSocketClient *) kClient)->send(
                    "回调调用:" + url_path + " " + method + " " + ((kWebSocketClient *) kClient)->header["host"]) >= 0 ? 0
                                                                                                                   : -1;
            */
            return ((kWebSocketClient *) kClient)->send(data, type) >= 0 ? 0 : -1;
        } else if (type == 8) {
            int _fd = ((kWebSocketClient *) kClient)->get_fd();
            // 关闭连接，可用于释放
            return 0;
        } else {
            return -1;
        }
        // return ((kWebSocketClient *) kClient)->send(data, type) >= 0;
    }, "/ws");
    kProxy.listen(20, port, ip.c_str());
#ifdef WIN32
    WSACleanup();
#endif
    return 0;
}