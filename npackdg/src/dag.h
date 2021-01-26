#ifndef DAG_H
#define DAG_H

#include <vector>

/**
 * @brief directed acyclic graph
 */
class DAG
{
    // array containing adjacency lists
    std::vector<std::vector<int>> adj;
public:
    DAG();

    /**
     * @brief computes a topological sort of the complete graph using indegrees
     * @return
     */
    std::vector<int> topologicalSort();

    /**
     * @brief function to add an edge to graph
     * @param u from this edge
     * @param v to this edge
     */
    void addEdge(int u, int v);

    /// <summary>Ermittelt die benachbarten Knoten</summary>
    ///
    /// <param name="U">ID eines Knoten: 0, 1, 2, ...</param>
    /// <returns>
    /// die IDs der Knoten. Dieser Array darf nicht geändert werden.
    /// </returns>
    ///

    /**
     * @brief getEdges returns the adjacent nodes
     * @param u a node
     * @return adjacent nodes
     */
    std::vector<int> getEdges(int u);
};

#endif // DAG_H
