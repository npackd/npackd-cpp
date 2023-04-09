#include "dag.h"

#include <algorithm>
#include <stdexcept>

void DAG::addEdge(int u, int v)
{
    int m = std::max(u, v);

    if (static_cast<int>(adj.size()) <= m)
        adj.resize(m + 1);

    adj[u].push_back(v);
}

std::vector<int> DAG::topologicalSort() const
{
    std::vector<int> inDegree;

    // https://www.geeksforgeeks.org/topological-sorting-indegree-based-solution/

    // create a vector to store indegrees of all
    // vertices. Initialize all indegrees as 0.
    inDegree.resize(adj.size());

    // traverse adjacency lists to fill indegrees of
    // vertices. This step takes O(V+E) time
    for (int u = 0; u < static_cast<int>(adj.size()); ++u) {
        for (int j = 0; j < static_cast<int>(adj[u].size()); ++j) {
            inDegree[adj[u][j]]++;
        };
    };

    // Create an queue and enqueue all vertices with indegree 0
    std::vector<int> q;
    for (int i = 0; i < static_cast<int>(adj.size()); ++i) {
        if (inDegree[i] == 0) {
            q.push_back(i);
        };
    };

    // initialize count of visited vertices
    int cnt = 0;

    std::vector<int> result;

    // one by one dequeue vertices from queue and enqueue
    // adjacents if indegree of adjacent becomes 0
    while (q.size() > 0) {
        // Extract front of queue
        // (or perform dequeue)
        // and add it to topological order
        int u = q.front();
        q.erase(q.begin());

        result.push_back(u);

        // iterate through all its neighbouring nodes
        // of dequeued node u and decrease their in-degree by 1
        for (int i = 0; i < static_cast<int>(adj[u].size()); ++i) {
            int node = adj[u][i];
            --inDegree[node];

            // if in-degree becomes zero,
            // add it to queue
            if (inDegree[node] == 0) {
                q.push_back(node);
            };
        };

        ++cnt;
    };

    // check if there was a cycle
    if (cnt != static_cast<int>(adj.size()))
        throw std::domain_error("There exists a cycle in the graph");

    return result;
}

void DAG::swapNodes(int a, int b)
{
    auto tmp = adj[a];
    adj[a] = adj[b];
    adj[b] = tmp;

    for (int i = 0; i < static_cast<int>(adj.size()); ++i) {
        for (int j = 0; j < static_cast<int>(adj[i].size()); ++j) {
            if (adj[i][j] == a)
                adj[i][j] = b;
            else if (adj[i][j] == b)
                adj[i][j] = a;
        };
    };
}

const std::vector<int>& DAG::getEdges(int u) const
{
    return adj[u];
}

void DAG::resize(size_t count)
{
    this->adj.resize(count);
}

size_t DAG::size() const
{
    return this->adj.size();
}

