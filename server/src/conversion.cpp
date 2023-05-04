//
// Created by Qiu Yuqing on 2023/4/28.
//
#include <deque>
#include <fstream>
#include <sstream>
#include <math.h>
#include <algorithm>
#include <omp.h>

#include "conversion.h"
#include "unionFind.h"

int Conversion::getPixel(int x, int y) {
    return pixelToNode[y * w + x];
}

void Conversion::setPixel(int x, int y, int id) {
    pixelToNode[y * w + x] = id;
}

void Conversion::printPixelToNodes() {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (getPixel(x, y) != -2) {
                std::cout << getPixel(x, y) << " ";
            } else {
                std::cout << "| ";
            }
        }
        std::cout << std::endl;
    }
}

int Conversion::loadFromFile(std::string &fileName) {
    std::ifstream inFile;
    inFile.open(fileName);
    if (!inFile) {
        return FAILURE;
    }
    std::string line;

    // read w and h and initialize pixelToNode to w * h
    std::getline(inFile, line);
    std::stringstream sstream_w(line);
    std::string str;
    std::getline(sstream_w, str, '\n');
    w = (int)atoi(str.c_str());
    std::getline(inFile, line);
    std::stringstream sstream_h(line);
    std::getline(sstream_h, str, '\n');
    h = (int)atoi(str.c_str());

    // read nodes map
    while (std::getline(inFile, line)) {
        // skip empty line
        if (line.empty()) {
            continue;
        }

        std::stringstream sstream(line);
        std::string str;
        std::getline(sstream, str, '\n');
        pixelToNode.push_back((int)atoi(str.c_str()));
    }
    inFile.close();
    return SUCCESS;
}

void Conversion::saveToFile(std::string &fileName) {
    std::ofstream outFile(fileName);
    if (!outFile) {
        std::cout << "error writing file \"" << fileName << "\"" << std::endl;
        return;
    }
    outFile << nodeNum << std::endl;
    for (auto &e : edges) {
        outFile << e.first << " " << e.second << std::endl;
    }
    outFile.close();
    if (!outFile)
        std::cout << "error writing file \"" << fileName << "\"" << std::endl;
}

void Conversion::saveNodesMapToFile(std::string &fileName) {
    std::ofstream outFile(fileName);
    if (!outFile) {
        std::cout << "error writing file \"" << fileName << "\"" << std::endl;
        return;
    }
    outFile << w << std::endl;
    outFile << h << std::endl;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            outFile << getPixel(x, y) << "\t";
        }
        outFile << std::endl;
    }
    outFile.close();
    if (!outFile)
        std::cout << "error writing file \"" << fileName << "\"" << std::endl;
}

void Conversion::saveNodesMapToFilePar(std::string &fileName) {
    std::ofstream outFile(fileName);
    if (!outFile) {
        std::cout << "error writing file \"" << fileName << "\"" << std::endl;
        return;
    }
    outFile << w << std::endl;
    outFile << h << std::endl;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            outFile << getPixelPar(x, y) << "\t";
        }
        outFile << std::endl;
    }
    outFile.close();
    if (!outFile)
        std::cout << "error writing file \"" << fileName << "\"" << std::endl;
}

void Conversion::findNodesSeq() {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (getPixel(x, y) == -1) {
                std::vector<Point> localMarginalPoints;
                fillAreaSeq(x, y, nodeNum, localMarginalPoints);
                if (!localMarginalPoints.empty()) {
                    nodeNum++;
                    marginalPoints.push_back(localMarginalPoints);
                }
            }
        }
    }

    // for debug
    std::string fileNameSeq("nodesMap-seq-step7.txt");
    saveNodesMapToFile(fileNameSeq);
}

void Conversion::splitNodesMap() {
    for (int gridIdxY = 0; gridIdxY < GRID_DIM; ++gridIdxY) {
        for (int gridIdxX = 0; gridIdxX < GRID_DIM; ++gridIdxX) {
            int gridGlobalId = gridIdxY * GRID_DIM + gridIdxX;
            pixelToNodePar.push_back(std::vector<int>());
            int localW = getGridWidth(gridIdxX);
            int localH = getGridHeight(gridIdxY);
            for (int localY = 0; localY < localH; ++localY) {
                for (int localX = 0; localX < localW; ++localX) {
                    int pixelGlobalIdxX = getGlobalX(gridIdxX, localX);
                    int pixelGlobalIdxY = getGlobalY(gridIdxY, localY);
                    int nodeId = getPixel(pixelGlobalIdxX, pixelGlobalIdxY);
                    // printf("in grid (%d, %d), local pixel (%d, %d) has global idx (%d, %d) and nodeId %d\n",
                    //         gridIdxX, gridIdxY, localX, localY, pixelGlobalIdxX, pixelGlobalIdxY, nodeId);
                    pixelToNodePar[gridGlobalId].push_back(nodeId);
                }
            }
        }
    }
}

void Conversion::findNodesPar() {
    // step 1: convert nodes_map to 4d array
    splitNodesMap();

    // for debug
    std::string fileNamePar("nodesMap-par-step1.txt");
    saveNodesMapToFilePar(fileNamePar);
    std::cout << "saved nodes map to file!" << std::endl;

    std::vector<std::vector<std::vector<Point>>> marginalPointsPerGrid(GRID_DIM * GRID_DIM);
    encodedNodeIdPerGrid.resize(GRID_DIM * GRID_DIM);
    conflictPairsPerGrid.resize(GRID_DIM * GRID_DIM);

    #pragma omp parallel for schedule(dynamic, 1) 
    for (int threadId = 0; threadId < GRID_DIM * GRID_DIM; ++threadId) {
        // step 2: use openmp to let different nodes process individual grids
        findNodesForGrid(threadId, marginalPointsPerGrid[threadId], encodedNodeIdPerGrid[threadId]);
        
        // // step 3: find node idx pairs that belong to the same global node in parallel
        // findConflictPairsForGrid(threadId, conflictPairsPerGrid[threadId], marginalPointsPerGrid[threadId]);
    }

    // step 4: build a global UnionFind using conflictPairsPerGrid, and finalize node ids
    calGlobalIdx();
    
    // step 5: let each grid updates its node ids in parallel using omp
    // may also convert 4d array back to 2d array in this step
    #pragma omp parallel for schedule(dynamic, 1) shared(nodeIdMapping)
    for (int threadId = 0; threadId < GRID_DIM * GRID_DIM; ++threadId) {
        updateNodeIpForGrid(threadId);
    }

    // step 6: update the global marginal points
    updateGlobalMarginalPoints(marginalPointsPerGrid);
}

void Conversion::updateGlobalMarginalPoints(std::vector<std::vector<std::vector<Point>>> &marginalPointsPerGrid) {
    for (int gridIdxY = 0; gridIdxY < GRID_DIM; ++gridIdxY) {
        for (int gridIdxX = 0; gridIdxX < GRID_DIM; ++gridIdxX) {
            // each grid
            int gridGlobalId = gridIdxY * GRID_DIM + gridIdxX;
            auto &gridMarginalPoints = marginalPointsPerGrid[gridGlobalId];
            for (auto &localMarginalPoints : gridMarginalPoints) {
                // each margin
                Point &sampleLocation = localMarginalPoints.at(0);
                int nodeId = getPixelPar(gridIdxX, gridIdxY, sampleLocation.x, sampleLocation.y);
                int encodedNodeId = encodeNodeId(gridIdxX, gridIdxY, nodeId);
                int globalNodeId = nodeIdMapping.at(encodedNodeId);
                for (auto &marginalPoint : localMarginalPoints) {
                    // each point
                    marginalPoints.at(globalNodeId).push_back(marginalPoint);
                }
            }
        }
    }
}

void Conversion::updateNodeIpForGrid(int threadId) {
    int gridIdxX = threadId % GRID_DIM;
    int gridIdxY = threadId / GRID_DIM;
    int localW = getGridWidth(gridIdxX);
    int localH = getGridHeight(gridIdxY);

    for (int localY = 0; localY < localH; ++localY) {
        for (int localX = 0; localX < localW; ++localW) {
            int encodedNodeId = encodeNodeId(gridIdxX, gridIdxY, getPixelPar(gridIdxX, gridIdxY, localX, localY));
            int newNodeId = nodeIdMapping.at(encodedNodeId);
            setPixelPar(gridIdxX, gridIdxY, localX, localY, newNodeId);
        }
    }
}

void Conversion::findConflictPairsForGrid(int threadId, pair_set &gridNodePairs, std::vector<std::vector<Point>> &gridMarginalPoints) {
    // TODO: find node pairs and update marginal points
    int gridIdxX = threadId % GRID_DIM;
    int gridIdxY = threadId / GRID_DIM;
    int localW = getGridWidth(gridIdxX);
    int localH = getGridHeight(gridIdxY);

    // lower boundary
    if (gridIdxY < GRID_DIM - 1) {
        int localY = localH - 1;
        for (int localX = 0; localX < localW; ++localX) {
            int localNodeId = getPixelPar(gridIdxX, gridIdxY, localX, localY);
            if (localNodeId == -2)  // is a node boundary, skip
                continue;

            int lowerNodeId = getPixelPar(gridIdxX, gridIdxY + 1, localX, 0);
            if (lowerNodeId == -2)  // a new marginal point
                gridMarginalPoints[localNodeId].push_back(Point{localX, localY});
            else if (lowerNodeId >= 0 && lowerNodeId != localNodeId) {  // a node pair found
                gridNodePairs.insert({
                    encodeNodeId(gridIdxX, gridIdxY, localNodeId),
                    encodeNodeId(gridIdxX, gridIdxY + 1, lowerNodeId)
                });
            }
        }
    }
    // right boundary
    if (gridIdxX < GRID_DIM - 1) {
        int localX = localW - 1;
        for (int localY = 0; localY < localH; ++localY) {
            int localNodeId = getPixelPar(gridIdxX, gridIdxY, localX, localY);
            if (localNodeId == -2)  // is a node boundary, skip
                continue;

            int rightNodeId = getPixelPar(gridIdxX + 1, gridIdxY, 0, localY);
            if (rightNodeId == -2)  // a new marginal point
                gridMarginalPoints[localNodeId].push_back(Point{localX, localY});
            else if (rightNodeId >= 0 && rightNodeId != localNodeId) {  // a node pair found
                gridNodePairs.insert({
                    encodeNodeId(gridIdxX, gridIdxY, localNodeId),
                    encodeNodeId(gridIdxX + 1, gridIdxY, rightNodeId)
                });
            }
        }
    }
}

void Conversion::findNodesForGrid(int threadId, std::vector<std::vector<Point>> &gridMarginalPoints, std::vector<int> &gridEncodedNodeIds) {
    int gridIdxX = threadId % GRID_DIM;
    int gridIdxY = threadId / GRID_DIM;
    int localW = getGridWidth(gridIdxX);
    int localH = getGridHeight(gridIdxY);
    int localNodeNum = 0;
    for (int y = 0; y < localH; y++) {
        for (int x = 0; x < localW; x++) {
            if (getPixelPar(gridIdxX, gridIdxY, x, y) == -1) {
                std::vector<Point> localMarginalPoints;
                fillAreaPar(gridIdxX, gridIdxY, localW, localH, x, y, nodeNum, localMarginalPoints);
                if (!localMarginalPoints.empty()) {
                    gridEncodedNodeIds.push_back(encodeNodeId(gridIdxX, gridIdxY, localNodeNum));
                    localNodeNum++;
                    gridMarginalPoints.push_back(localMarginalPoints);
                }
            }
        }
    }
}

void Conversion::fillAreaSeq(int x, int y, int id, std::vector<Point> &localMarginalPoints) {
    int n = 0;
    std::vector<std::vector<bool>> visited(w, std::vector<bool>(h, false));
    std::deque<Point> qu;
    qu.emplace_front(Point{x, y});

    std::vector<std::pair<int,int>> dirs = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

    while (!qu.empty()) {
        // pop point from list and color it
        Point p = qu.front();
        qu.pop_front();
        bool is_marginal = false;
        setPixel(p.x, p.y, id);
        n += 1;

        // check neighbors
        for (auto &dir : dirs) {
            int nx = p.x + dir.first;
            int ny = p.y + dir.second;
            // check if marginal
            if (getPixel(nx, ny) == -2)
                is_marginal = true;
            // skip if visited
            if (visited[nx][ny]) {
                continue;
            }
            if (getPixel(nx, ny) == -1) {
                qu.push_front({nx, ny});
            }
            visited[nx][ny] = true;
        }

        // check if marginal
        if (is_marginal)
            localMarginalPoints.push_back(p);
    }
    // one-pixel bug: if only one pixel, don't count it as separate area
    if (n == 1) {
        setPixel(x, y, -2);
        localMarginalPoints.clear();
    }
}

void Conversion::fillAreaPar(int gridIdxX, int gridIdxY,
                                           int localW, int localH,
                                           int x, int y, int id,
                                           std::vector<Point> &localMarginalPoints
                                           ) {
    int n = 0;
    std::vector<std::vector<bool>> visited(w, std::vector<bool>(h, false));
    std::deque<Point> qu;
    qu.emplace_front(Point{x, y});

    std::vector<std::pair<int,int>> dirs = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

    while (!qu.empty()) {
        // pop point from list and color it
        Point p = qu.front();
        qu.pop_front();
        bool is_marginal = false;
        setPixelPar(gridIdxX, gridIdxY, p.x, p.y, id);
        n += 1;

        // check neighbors
        for (auto &dir : dirs) {
            int nx = p.x + dir.first;
            int ny = p.y + dir.second;
            // check if out of boundary
            if (nx < 0 || nx >= localW || ny < 0 || ny >= localH)
                continue;
            // check if marginal
            if (getPixel(nx, ny) == -2)
                is_marginal = true;
            // skip if visited
            if (visited[nx][ny]) {
                continue;
            }
            if (getPixelPar(gridIdxX, gridIdxY, nx, ny) == -1) {
                qu.push_front({nx, ny});
            }
            visited[nx][ny] = true;
        }

        // check if marginal
        if (is_marginal)
            localMarginalPoints.push_back(p);   // TODO: need to change local p to global P?
    }
    // one-pixel bug: if only one pixel, don't count it as separate area
    if (n == 1) {
        setPixel(x, y, -2);
        localMarginalPoints.clear();
    }
}

int Conversion::getPixelPar(int gridIdxX, int gridIdxY, int x, int y) {
    int gridGlobalId = gridIdxY * GRID_DIM + gridIdxX;
    int gridW = getGridWidth(gridIdxX);
    int pixelLocalId = y * gridW + x;
    return pixelToNodePar[gridGlobalId][pixelLocalId];
}

int Conversion::getPixelPar(int globalX, int globalY) {
    int gridIdxX = getGridIdxX(globalX);
    int gridIdxY = getGridIdxY(globalY);
    int localX = getLocalX(globalX);
    int localY = getLocalY(globalY);
    return getPixelPar(gridIdxX, gridIdxY, localX, localY);
}

void Conversion::setPixelPar(int gridIdxX, int gridIdxY, int x, int y, int id) {
    int gridGlobalId = gridIdxY * GRID_DIM + gridIdxX;
    int gridW = getGridWidth(gridIdxX);
    int pixelLocalId = y * gridW + x;
    pixelToNodePar[gridGlobalId][pixelLocalId] = id;
}

void Conversion::setPixelPar(int globalX, int globalY, int id) {
    int gridIdxX = getGridIdxX(globalX);
    int gridIdxY = getGridIdxY(globalY);
    int localX = getLocalX(globalX);
    int localY = getLocalY(globalY);
    setPixelPar(gridIdxX, gridIdxY, localX, localY, id);
}

int Conversion::getGridWidth(int gridIdxX) {
    int quotient = w / GRID_DIM;
    int remainder = w % GRID_DIM;
    if (gridIdxX < GRID_DIM - remainder)
        return quotient;
    else
        return quotient + 1;
}

int Conversion::getGridHeight(int gridIdxY) {
    int quotient = h / GRID_DIM;
    int remainder = h % GRID_DIM;
    if (gridIdxY < GRID_DIM - remainder)
        return quotient;
    else
        return quotient + 1;
}

int Conversion::encodeNodeId(int gridIdxX, int gridIdxY, int nodeId) {
    int gridGlobalId = gridIdxY * GRID_DIM + gridIdxX;
    return (gridGlobalId << 16) + nodeId;
}

int Conversion::getGlobalX(int gridIdxX, int localX) {
    int quotient = w / GRID_DIM;
    int remainder = w % GRID_DIM;
    int baseGlobalX;
    if (gridIdxX <= GRID_DIM - remainder)
        baseGlobalX = gridIdxX * quotient;
    else
        baseGlobalX = gridIdxX * quotient + gridIdxX - GRID_DIM + remainder;
    return baseGlobalX + localX;
}

int Conversion::getGlobalY(int gridIdxY, int localY) {
    int quotient = h / GRID_DIM;
    int remainder = h % GRID_DIM;
    int baseGlobalY;
    if (gridIdxY <= GRID_DIM - remainder)
        baseGlobalY = gridIdxY * quotient;
    else
        baseGlobalY = gridIdxY * quotient + gridIdxY - GRID_DIM + remainder;
    return baseGlobalY + localY;
}

int Conversion::getGridIdxX(int globalX) {
    int quotient = w / GRID_DIM;
    int remainder = w % GRID_DIM;
    int division_point = (GRID_DIM - remainder) * quotient - 1;

    if (globalX <= division_point) {
        return globalX / quotient;
    }
    int extra = globalX - division_point;
    int idxExtra = extra / (quotient + 1);
    return GRID_DIM - remainder + idxExtra - 1;
}

int Conversion::getGridIdxY(int globalY) {
    int quotient = h / GRID_DIM;
    int remainder = h % GRID_DIM;
    int division_point = (GRID_DIM - remainder) * quotient - 1;

    if (globalY <= division_point) {
        return globalY / quotient;
    }
    int extra = globalY - division_point;
    int idxExtra = extra / (quotient + 1);
    return GRID_DIM - remainder + idxExtra - 1;
}

int Conversion::getLocalX(int globalX) {
    int gridIdxX = getGridIdxX(globalX);
    return globalX - getGlobalX(gridIdxX, 0);
}

int Conversion::getLocalY(int globalY) {
    int gridIdxY = getGridIdxX(globalY);
    return globalY - getGlobalY(gridIdxY, 0);
}

double getDistance(int x1, int y1, int x2, int y2) {
    return sqrt((double)(x1 - x2) * (double)(x1 - x2) + (double)(y1 - y2) * (double)(y1 - y2));
}

void Conversion::findEdgesSeq() {
    std::vector<std::unordered_set<int>> adjacentLists(nodeNum);
    // compare and check if nodes have an edge
    for (int i = 0; i < nodeNum; i++) {
        std::unordered_set<int> visited_neighbors; // neighbors that have been visited
        std::unordered_map<int,int> tmpEdges; // check potential edges
        std::vector<Point>& mp = marginalPoints[i];
        for (Point &p : mp) {
            // check the surrounding points of p to see if they belong to other nodes
            for (int k = -MAX_LINE_THICKNESS; k < MAX_LINE_THICKNESS; k++) {
                for (int l = -MAX_LINE_THICKNESS; l < MAX_LINE_THICKNESS; l++) {
                    int tmpx = p.x + k;
                    int tmpy = p.y + l;

                    // if out of boundary, skip
                    if (tmpx < 0 || tmpx >= w || tmpy < 0 || tmpy >= h) {
                        continue;
                    }

                    // if out of distance, skip
                    if (getDistance(tmpx, tmpy, p.x, p.y) >= (double)MAX_LINE_THICKNESS)
                        continue;
                    int global_idx = tmpy * w + tmpx;

                    // if already visited, skip
                    if (visited_neighbors.count(global_idx))
                        continue;
                    visited_neighbors.insert(global_idx);

                    int tmpId = getPixel(tmpx, tmpy);
                    if (tmpId >= 0 && tmpId != i) {
                        // if already added as an edge, skip
                        if (adjacentLists[i].count(tmpId))
                            continue;
                        // if already added as a temporary edge, increment the counter
                        if (tmpEdges.count(tmpId)) {
                            tmpEdges[tmpId] += 1;
                            if (tmpEdges[tmpId] > EDGE_THRESHOLD) {
                                adjacentLists[i].insert(tmpId);
                                adjacentLists[tmpId].insert(i);
                            }
                        } else {
                            // add the edge to temp edges array
                            tmpEdges.insert({tmpId, 1});
                        }
                    }
                }
            }
        }
    }

    // convert adjacentLists to Edges
    for (int i = 0; i < nodeNum; i++) {
        auto tmp = adjacentLists[i];
        std::vector<int> neighbors;
        neighbors.assign(tmp.begin(), tmp.end());
        std::sort(neighbors.begin(), neighbors.end());
        for (auto &j : neighbors) {
            if (j > i) {
                edges.emplace_back(i, j);
            }
        }
    }
}

void Conversion::findEdgesPar() {
    std::vector<std::unordered_set<int>> adjacentLists(nodeNum);
#pragma omp parallel for default(none) schedule(dynamic, 10) shared(adjacentLists, marginalPoints)
    // compare and check if nodes have an edge
    for (int i = 0; i < nodeNum; i++) {
        std::unordered_set<int> visited_neighbors; // neighbors that have been visited
        std::unordered_map<int,int> tmpEdges; // track potential edges
        std::vector<Point>& mp = marginalPoints[i];
        for (Point &p : mp) {
            // check the surrounding points of p to see if they belong to other nodes
            for (int k = -MAX_LINE_THICKNESS; k < MAX_LINE_THICKNESS; k++) {
                for (int l = -MAX_LINE_THICKNESS; l < MAX_LINE_THICKNESS; l++) {
                    int tmpx = p.x + k;
                    int tmpy = p.y + l;

                    // if out of boundary, skip
                    if (tmpx < 0 || tmpx >= w || tmpy < 0 || tmpy >= h) {
                        continue;
                    }

                    // if out of distance, skip
                    if (getDistance(tmpx, tmpy, p.x, p.y) >= (double)MAX_LINE_THICKNESS)
                        continue;
                    int global_idx = tmpy * w + tmpx;

                    // if already visited, skip
                    if (visited_neighbors.count(global_idx))
                        continue;
                    visited_neighbors.insert(global_idx);

                    int tmpId = getPixelPar(tmpx, tmpy);
                    if (tmpId >= 0 && tmpId != i) {
                        // if already added as an edge, skip
                        size_t exist;
#pragma omp critical
                        {
                            exist = adjacentLists[i].count(tmpId);
                        }
                        if (exist)
                            continue;
                        // if already added as a temporary edge, increment the counter
                        if (tmpEdges.count(tmpId)) {
                            tmpEdges[tmpId] += 1;
                            if (tmpEdges[tmpId] > EDGE_THRESHOLD) {
#pragma omp critical
                                {
                                    adjacentLists[i].insert(tmpId);
                                    adjacentLists[tmpId].insert(i);
                                }

                            }
                        } else {
                            // add the edge to temp edges array
                            tmpEdges.insert({tmpId, 1});
                        }
                    }
                }
            }
        }
    }

    // convert adjacentLists to Edges
    for (int i = 0; i < nodeNum; i++) {
        auto tmp = adjacentLists[i];
        std::vector<int> neighbors;
        neighbors.assign(tmp.begin(), tmp.end());
        std::sort(neighbors.begin(), neighbors.end());
        for (auto &j : neighbors) {
            if (j > i) {
                edges.emplace_back(i, j);
            }
        }
    }
}

void Conversion::findNodes() {
    if (seq) {
        findNodesSeq();
    } else {
        findNodesPar();
    }
}

void Conversion::findEdges() {
    if (seq) {
        findEdgesSeq();
    } else {
        findEdgesPar();
    }
}

void Conversion::addMapColors(const std::vector<int>& colors) {
    // update pixelToNode with colors
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int id = getPixel(x, y);
            if (id >= 0) {
                setPixel(x, y, colors[id]);
            }
        }
    }
}

void Conversion::calGlobalIdx() {
    unionFind uf(encodedNodeIdPerGrid);
    for (auto &v : conflictPairsPerGrid) {
        for (auto &p : v) {
            uf.unionPair(p.first, p.second);
        }
    }
    uf.compressMapping();
    nodeIdMapping = uf.getMapping();
}


