#ifndef DAG_H
#define DAG_H

#include <vector>
#include <cstddef>

/**
 * @brief directed acyclic graph
 */
class DAG
{
    // array containing adjacency lists
    std::vector<std::vector<int>> adj;
public:
    /**
     * @brief computes a topological sort of the complete graph using indegrees
     * @return
     */
    std::vector<int> topologicalSort() const;

    /**
     * @brief swaps (renames) the nodes
     * @param a first node
     * @param b second node
     */
    void swapNodes(int a, int b);

    /**
     * @brief function to add an edge to graph
     * @param u from this edge
     * @param v to this edge
     */
    void addEdge(int u, int v);

    /**
     * @brief getEdges returns the adjacent nodes
     * @param u a node
     * @return adjacent nodes
     */
    const std::vector<int> &getEdges(int u) const;

    /**
     * @brief changes the number of nodes
     * @param count new number of nodes
     */
    void resize(size_t count);

    /**
     * @return number of nodes
     */
    size_t size() const;
};

#endif // DAG_H
