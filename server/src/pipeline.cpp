#include "pipeline.h"
#include "fourColor.h"
#include "conversion.h"

/**
 * convert a string representing nodes map values to an int array
 */
static void constructInputArray(char *input, std::vector<int> &array) {
    char *itr = input, *mark = input;
    char delimeter = ',';
    unsigned counter = 0;
    while (counter < array.size()) {
        while (*itr != delimeter)   itr ++;
        *itr = 0;

        array[counter++] = atoi(mark);

        itr ++;
        mark = itr;
    }
}


/**
 * convert an int array to a string representing nodes map
 */
static void constructOutputArray(std::vector<int> &array, char **output, size_t *solution_lenp) {
    std::string output_str;
    for (int val : array)
        output_str += std::to_string(val) + ",";

    // get rid of the last comma
    size_t length = output_str.size() - 1;
    *solution_lenp = length;
    char *solution_cstr = (char *) output_str.c_str();
    *output = (char *) Malloc(length);
    memcpy(*output, solution_cstr, length);
}


/**
 * map solver
 * @param input[in] the input of the map data
 * @param solutionp[out] a pointer to the buffer to store the solution (needs to be allocated in this function)
 * @param solution_lenp[out] a pointer to the length of the solution string (whose value needs to be updated in this function)
 */
bool solveMap(char *input, char **solutionp, size_t *solution_lenp) {
    char *itr = input, *mark = input, *nodes_map_str = NULL;
    bool seq;
    // seq
    while (*itr != '\n')    itr ++;
    *itr = 0;
    if (strcmp(mark, "true") == 0)
        seq = true;
    else
        seq = false;
    itr ++;
    mark = itr;
    // w
    while (*itr != '\n')    itr ++;
    *itr = 0;
    int w = atoi(mark);
    itr ++;
    mark = itr;
    // h
    while (*itr != '\n')    itr ++;
    *itr = 0;
    int h = atoi(mark);
    itr ++;
    nodes_map_str = itr;

    sio_printf("width = %d, height = %d\n", w, h);

    size_t length = w * h;
    std::vector<int> input_pixels(length);
    std::vector<int> output_pixels(length);
    constructInputArray(nodes_map_str, input_pixels);

    bool res = solveMapHelper(seq, w, h, input_pixels, output_pixels);

    if (res)    constructOutputArray(output_pixels, solutionp, solution_lenp);
    return res;
}


/**
 * map solver
 * @param w[in] the width of the image
 * @param h[in] the height of the image
 * @param seq[in] whether or not to solve sequentially
 * @param input[in] the array of nodes map (length: w * h)
 * @param output[out] the array of nodes map (length: w * h)
 *
 * @return true if solving succeeds, false otherwise
 */
bool solveMapHelper(bool seq, int w, int h, const std::vector<int> &input, std::vector<int> &output) {
    std::vector<std::string> return_status_array = {"Success", "Timeout", "Failure", "Wrong"};
    int timeout = 10;
    bool converter_seq = seq;
    bool solver_seq = seq;
//    std::string adjFile = "adj.txt";
//    std::string colorFile = "colors.txt";
//    std::string nodesMapFile = "nodesMap.txt";

    if (seq)
        printf("solving sequentially\n");
    else
        printf("solving in parallel\n");
    
    Conversion converter(converter_seq);
    fourColorSolver solver(timeout, solver_seq);

    converter.setPixelToNodeArray(w, h, input);

    converter.findNodes();
    converter.findEdges();

    solver.setGraph(converter.getNodeNum(), converter.getEdges());
//    solver.saveNodeAdjListToFile(adjFile);
    
    int rst = solver.solveGraph();
    std::cout << "INFO: Solve Graph: " << return_status_array[rst] << "\n";
//    solver.saveToFile(colorFile);

    converter.addMapColors(solver.getColors());
//    converter.saveNodesMapToFile(nodesMapFile);

    output = converter.getPixelToNodeArray();
    return true;
}