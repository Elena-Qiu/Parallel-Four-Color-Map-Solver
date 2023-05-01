//
// Created by Qiu Yuqing on 2023/4/28.
//

#include <vector>
#include <iostream>
//#include "fourColor.h"

#ifndef INC_15618_FINAL_PROJECT_CONVERSION_H
#define INC_15618_FINAL_PROJECT_CONVERSION_H

using time_point = std::chrono::high_resolution_clock::time_point;

enum return_status {SUCCESS, TIMEOUT, FAILURE, WRONG};
static const std::vector<std::string> return_status_array = {"Success", "Timeout", "Failure", "Wrong"};

const int LINE_EXPANSION = 0;
const int MAX_LINE_THICKNESS = 2 * LINE_EXPANSION + 3;
const int EDGE_THRESHOLD = 3;

typedef struct Point {
    int x;
    int y;
}point_t;

class Conversion {
public:
    bool testMode = true;

    // constructor
    explicit Conversion(int timeOut = 10) {
        w = 0;
        h = 0;
        nodeNum = 0;
//        graphSolver = fourColorSolver(timeOut);
    }

    // file input and output
    int loadFromFile(std::string &fileName);
    void saveToFile(std::string &fileName);

    // API input and output
    void setPixelToNodeArray(int _w, int _h, const std::vector<int>& arr) {
        w = _w;
        h = _h;
        pixelToNode = arr;
    }
    std::vector<int>& getPixelToNodeArray() {
        return pixelToNode;
    }

    // algorithm
    int solveMap();

    // debug
    void printPixelToNodes();

private:
    int w;
    int h;
    int nodeNum;
    std::vector<int> pixelToNode;
    std::vector<std::vector<Point>> marginalPoints;
    std::vector<std::pair<int,int>> edges;
//    fourColorSolver graphSolver;

    int getPixel(int x, int y);
    void setPixel(int x, int y, int id);

    void findNodes();
    std::vector<Point> fillArea(int x, int y, int id);
    void findEdges();
};


#endif //INC_15618_FINAL_PROJECT_CONVERSION_H
