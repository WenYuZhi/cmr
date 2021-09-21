#define CMR_DEBUG /* Uncomment to debug this file. */

#include "regular_internal.h"

#include <cmr/graphic.h>
#include <cmr/network.h>

#include "env_internal.h"
#include "hashtable.h"

#include <stdint.h>

/**
 * \brief Recursive DFS for finding all articulation points of a graph.
 */

static
int dfsArticulationPoint(
  CMR_GRAPH* graph,             /**< Graph. */
  bool* edgesEnabled,           /**< Edge array indicating whether an edge is enabled. */
  CMR_GRAPH_NODE node,          /**< Current node. */
  bool* nodesVisited,           /**< Node array indicating whether a node was already visited. */
  int* nodesDiscoveryTime,      /**< Node array indicating at which time a node was visited. */
  int* ptime,                   /**< Pointer to current time. */
  CMR_GRAPH_NODE parentNode,    /**< Parent node in DFS arborescence. */
  size_t* nodesArticulationPoint  /**< Node array indicating whether a node is an articulation point. */
)
{
  assert(graph);
  assert(nodesVisited);
  assert(nodesDiscoveryTime);
  assert(ptime);
  assert(nodesArticulationPoint);

  size_t numChildren = 0;
  nodesVisited[node] = true;
  ++(*ptime);
  nodesDiscoveryTime[node] = *ptime;
  int earliestReachableTime = *ptime;

  for (CMR_GRAPH_ITER iter = CMRgraphIncFirst(graph, node); CMRgraphIncValid(graph, iter);
    iter = CMRgraphIncNext(graph, iter))
  {
    assert(CMRgraphIncSource(graph, iter) == node);
    if (!edgesEnabled[CMRgraphIncEdge(graph, iter)])
      continue;

    CMR_GRAPH_NODE v = CMRgraphIncTarget(graph, iter);
    if (!nodesVisited[v])
    {
      ++numChildren;
      int childEarliestReachableTime = dfsArticulationPoint(graph, edgesEnabled, v, nodesVisited, nodesDiscoveryTime,
        ptime, node, nodesArticulationPoint);
      if (childEarliestReachableTime < earliestReachableTime)
        earliestReachableTime = childEarliestReachableTime;
      if (parentNode >= 0 && childEarliestReachableTime >= nodesDiscoveryTime[node])
        nodesArticulationPoint[node] = true;
    }
    else if (v != parentNode && nodesDiscoveryTime[v] < earliestReachableTime)
      earliestReachableTime = nodesDiscoveryTime[v];
  }

  if (parentNode < 0)
  {
    if (numChildren > 1)
      nodesArticulationPoint[node] = true;
  } 

  return earliestReachableTime;
}

static
CMR_ERROR findArticulationPoints(
  CMR* cmr,                     /**< \ref CMR environment. */
  CMR_GRAPH* graph,             /**< Graph. */
  CMR_GRAPH_EDGE* columnEdges,  /**< Array with with map from columns to edges. */
  size_t* nodesArticulationPoint, /**< Node array indicating whether a node is an articulation point. */
  size_t* nonzeroColumns,       /**< Array with columns containing a nonzero. */
  size_t numNonzeroColumns      /**< Length of \p nonzeroColumns. */
)
{
  assert(graph);
  assert(columnEdges);
  assert(nodesArticulationPoint);
  assert(nonzeroColumns);

  bool* nodesVisited = NULL;
  CMR_CALL( CMRallocStackArray(cmr, &nodesVisited, CMRgraphMemNodes(graph)) );
  int* nodesDiscoveryTime = NULL;
  CMR_CALL( CMRallocStackArray(cmr, &nodesDiscoveryTime, CMRgraphMemNodes(graph)) );
  bool* edgesEnabled = NULL;
  CMR_CALL( CMRallocStackArray(cmr, &edgesEnabled, CMRgraphMemEdges(graph)) );

  for (CMR_GRAPH_NODE v = CMRgraphNodesFirst(graph); CMRgraphNodesValid(graph, v); v = CMRgraphNodesNext(graph, v))
  {
    nodesArticulationPoint[v] = false;
    nodesVisited[v] = false;
    nodesDiscoveryTime[v] = 0;
  }
  for (CMR_GRAPH_ITER iter = CMRgraphEdgesFirst(graph); CMRgraphEdgesValid(graph, iter);
    iter = CMRgraphEdgesNext(graph, iter))
  {
    edgesEnabled[CMRgraphEdgesEdge(graph, iter)] = true;
  }

  for (size_t i = 0; i < numNonzeroColumns; ++i)
    edgesEnabled[columnEdges[nonzeroColumns[i]]] = false;  

  int time = 0;
  dfsArticulationPoint(graph, edgesEnabled, CMRgraphNodesFirst(graph), nodesVisited, nodesDiscoveryTime, &time, -1,
    nodesArticulationPoint);

  CMR_CALL( CMRfreeStackArray(cmr, &edgesEnabled) );
  CMR_CALL( CMRfreeStackArray(cmr, &nodesDiscoveryTime) );
  CMR_CALL( CMRfreeStackArray(cmr, &nodesVisited) );

  return CMR_OKAY;
}

static
void dfsTree(
  CMR_GRAPH* graph,             /**< Graph. */
  bool* edgesTree,              /**< Edge array indicating whether an edge is enabled. */
  bool* nodesVisited,           /**< Node array indicating whether a node was already visited. */
  CMR_GRAPH_NODE* nodesParent,  /**< Node array indicating the parent node of each node. */
  CMR_GRAPH_NODE node           /**< Current node. */
)
{
  assert(graph);
  assert(edgesTree);
  assert(nodesVisited);
  assert(nodesParent);
  assert(node >= 0);

  nodesVisited[node] = true;
  for (CMR_GRAPH_ITER iter = CMRgraphIncFirst(graph, node); CMRgraphIncValid(graph, iter);
    iter = CMRgraphIncNext(graph, iter))
  {
    assert(CMRgraphIncSource(graph, iter) == node);
    if (edgesTree[CMRgraphIncEdge(graph, iter)])
    {
      CMR_GRAPH_NODE v = CMRgraphIncTarget(graph, iter);
      if (!nodesVisited[v])
      {
        nodesParent[v] = node;
        dfsTree(graph, edgesTree, nodesVisited, nodesParent, v);
      }
    }
  }
}

static
CMR_ERROR findTreeParents(
  CMR* cmr,                   /**< \ref CMR environment. */
  CMR_GRAPH* graph,           /**< Graph. */
  CMR_GRAPH_EDGE* rowEdges,   /**< Array with with map from rows to edges. */
  size_t numRows,             /**< Number of rows. */
  CMR_GRAPH_NODE* nodesParent /**< Array to be filled with map from nodes to parent nodes. */
)
{
  assert(graph);
  assert(rowEdges);
  assert(nodesParent);

  bool* nodesVisited = NULL;
  CMR_CALL( CMRallocStackArray(cmr, &nodesVisited, CMRgraphMemNodes(graph)) );

  bool* edgesTree = NULL;
  CMR_CALL( CMRallocStackArray(cmr, &edgesTree, CMRgraphMemEdges(graph)) );
  for (CMR_GRAPH_ITER iter = CMRgraphEdgesFirst(graph); CMRgraphEdgesValid(graph, iter);
    iter = CMRgraphEdgesNext(graph, iter))
  {
    edgesTree[ CMRgraphEdgesEdge(graph, iter) ] = false;
  }

  /* Enable tree edge. */
  for (size_t row = 0; row < numRows; ++row)
    edgesTree[ rowEdges[row] ] = true;

  CMR_GRAPH_NODE root = CMRgraphNodesFirst(graph);
  nodesParent[root] = -1;
  dfsTree(graph, edgesTree, nodesVisited, nodesParent, root);

  CMR_CALL( CMRfreeStackArray(cmr, &edgesTree) );
  CMR_CALL( CMRfreeStackArray(cmr, &nodesVisited) );

  return CMR_OKAY;
}

static
void dfsComponents(
  CMR_GRAPH* graph,
  bool* edgesEnabled,
  size_t* nodesComponent,
  CMR_GRAPH_NODE node,
  size_t component
)
{
  assert(graph);
  assert(edgesEnabled);
  assert(nodesComponent);
  assert(node >= 0);
  assert(component < SIZE_MAX);
  
  nodesComponent[node] = component;
  for (CMR_GRAPH_ITER iter = CMRgraphIncFirst(graph, node); CMRgraphIncValid(graph, iter);
    iter = CMRgraphIncNext(graph, iter))
  {
    assert(CMRgraphIncSource(graph, iter) == node);
    if (edgesEnabled[CMRgraphIncEdge(graph, iter)])
    {
      CMR_GRAPH_NODE v = CMRgraphIncTarget(graph, iter);
      if (nodesComponent[v] == SIZE_MAX)
        dfsComponents(graph, edgesEnabled, nodesComponent, v, component);
    }
  }
}

static
CMR_ERROR findComponents(
  CMR* cmr,                     /**< \ref CMR environment. */
  CMR_GRAPH* graph,             /**< Graph. */
  CMR_GRAPH_EDGE* columnEdges,  /**< Array with with map from columns to edges. */
  CMR_GRAPH_NODE removedNode,   /**< Node that shall be considered as removed. */
  size_t* nodesComponent,       /**< Node array indicating the components. */
  size_t* pnumComponents,       /**< Pointer for storing the number of connected components. */
  size_t* nonzeroColumns,       /**< Array with columns containing a nonzero. */
  size_t numNonzeroColumns      /**< Length of \p nonzeroColumns. */
)
{
  assert(graph);
  assert(columnEdges);
  assert(nonzeroColumns);

  bool* edgesEnabled = NULL;
  CMR_CALL( CMRallocStackArray(cmr, &edgesEnabled, CMRgraphMemEdges(graph)) );
  for (CMR_GRAPH_ITER iter = CMRgraphEdgesFirst(graph); CMRgraphEdgesValid(graph, iter);
    iter = CMRgraphEdgesNext(graph, iter))
  {
    edgesEnabled[ CMRgraphEdgesEdge(graph, iter) ] = true;
  }

  /* Disable edges around special node. */
  for (CMR_GRAPH_ITER iter = CMRgraphIncFirst(graph, removedNode); CMRgraphIncValid(graph, iter);
    iter = CMRgraphIncNext(graph, iter))
  {
    edgesEnabled[ CMRgraphIncEdge(graph, iter) ] = false;
  }

  /* Disable 1-edges. */
  for (size_t i = 0; i < numNonzeroColumns; ++i)
    edgesEnabled[ columnEdges[ nonzeroColumns[i] ] ] = false;

  /* Initialize components. */
  for (CMR_GRAPH_NODE v = CMRgraphNodesFirst(graph); CMRgraphNodesValid(graph, v); v = CMRgraphNodesNext(graph, v))
    nodesComponent[v] = SIZE_MAX;

  size_t component = 0;
  for (CMR_GRAPH_NODE source = CMRgraphNodesFirst(graph); CMRgraphNodesValid(graph, source);
    source = CMRgraphNodesNext(graph, source))
  {
    if (nodesComponent[source] == SIZE_MAX && source != removedNode)
    {
      dfsComponents(graph, edgesEnabled, nodesComponent, source, component);
      ++component;
    }
  }
  *pnumComponents = component;

  CMR_CALL( CMRfreeStackArray(cmr, &edgesEnabled) );

  return CMR_OKAY;
}

/**
 * \brief DFS for searching for a bipartition.
 */

static
bool dfsBipartite(
  CMR_GRAPH* graph,   /**< Graph. */
  bool* nodesVisited, /**< Node array indicating whether a node was visited already. */
  int* bipartition,   /**< Node array for storing the bipartition. */
  CMR_GRAPH_NODE node /**< Current node. */
)
{
  nodesVisited[node] = true;
  for (CMR_GRAPH_ITER iter = CMRgraphIncFirst(graph, node); CMRgraphIncValid(graph, iter);
    iter = CMRgraphIncNext(graph, iter))
  {
    assert(CMRgraphIncSource(graph, iter) == node);
    CMR_GRAPH_NODE v = CMRgraphIncTarget(graph, iter);
    if (nodesVisited[v])
    {
      if (bipartition[v] == bipartition[node])
        return false;
    }
    else
    {
      bipartition[v] = 1 - bipartition[node];
      bool isBipartite = dfsBipartite(graph, nodesVisited, bipartition, v);
      if (!isBipartite)
        return false;
    }
  }
  return true;
}

/**
 * \brief Finds a bipartition of a graph.
 */

static
CMR_ERROR findBipartition(
  CMR* cmr,           /**< \ref CMR environment. */
  CMR_GRAPH* graph,   /**< Graph. */
  int* bipartition,   /**< Node array indicating color class. */
  bool* pisBipartite  /**< Pointer for storing whether \p is bipartite. */
)
{
  assert(cmr);
  assert(graph);
  assert(bipartition);

  bool* nodesVisited = NULL;
  CMR_CALL( CMRallocStackArray(cmr, &nodesVisited, CMRgraphMemNodes(graph)) );
  for (CMR_GRAPH_NODE v = CMRgraphNodesFirst(graph); CMRgraphNodesValid(graph, v); v = CMRgraphNodesNext(graph, v))
    nodesVisited[v] = false;

  bool isBipartite = true;
  for (CMR_GRAPH_NODE source = CMRgraphNodesFirst(graph); CMRgraphNodesValid(graph, source );
    source = CMRgraphNodesNext(graph, source))
  {
    if (!nodesVisited[source])
    {
      bipartition[source] = 0;
      if (!dfsBipartite(graph, nodesVisited, bipartition, source))
      {
        isBipartite = false;
        break;
      }
    }
  }

  if (pisBipartite)
    *pisBipartite = isBipartite;

  CMR_CALL( CMRfreeStackArray(cmr, &nodesVisited) );

  return CMR_OKAY;
}


/**
 * \brief Extends \p graph for a submatrix augmented by 1 row.
 */

static
CMR_ERROR addToGraph1Row(
  CMR* cmr,                     /**< \ref CMR environment. */
  CMR_GRAPH* graph,             /**< Empty graph to be filled. */
  CMR_GRAPH_EDGE* rowEdges,     /**< Array to be filled with map from rows to edges. */
  CMR_GRAPH_EDGE* columnEdges,  /**< Array to be filled with map from columns to edges. */
  size_t baseNumRows,           /**< Number of rows already processed. */
  size_t baseNumColumns,        /**< Number of columns already processed. */
  size_t* nonzeroColumns,       /**< Array with columns containing a nonzero. */
  size_t numNonzeroColumns,     /**< Length of \p nonzeroColumns. */
  bool* pisGraphic              /**< Pointer for storing whether this extension was graphic. */
)
{
  assert(cmr);
  assert(graph);
  assert(baseNumRows >= 3);
  assert(baseNumColumns >= 3);
  assert(rowEdges);
  assert(columnEdges);
  assert(numNonzeroColumns <= baseNumColumns);

  size_t* nodesCandidate = NULL;
  CMR_CALL( CMRallocStackArray(cmr, &nodesCandidate, CMRgraphMemNodes(graph)) );

  CMR_CALL( findArticulationPoints(cmr, graph, columnEdges, nodesCandidate, nonzeroColumns, numNonzeroColumns) );
  // nodesParent
  
  size_t countCandidates = 0;
  for (CMR_GRAPH_NODE v = CMRgraphNodesFirst(graph); CMRgraphNodesValid(graph, v); v = CMRgraphNodesNext(graph, v))
  {
    if (nodesCandidate[v])
      ++countCandidates;
  }

  CMRdbgMsg(12, "Found %ld articulation points.\n", countCandidates);

  if (countCandidates > 0)
  {
    /* We need a rooted arborescence along the row (tree) edges. */
    CMR_GRAPH_NODE* nodesParent = NULL;
    CMR_CALL( CMRallocStackArray(cmr, &nodesParent, CMRgraphMemNodes(graph)) );
    CMR_CALL( findTreeParents(cmr, graph, rowEdges, baseNumRows, nodesParent) );

    /* Ensure that the fundamental cycles induced by the column-edges with a 1-entry go through the articular points. */
    CMR_GRAPH_NODE* nodeStacks[2] = { NULL, NULL };
    CMR_CALL( CMRallocStackArray(cmr, &nodeStacks[0], baseNumRows+1) );
    CMR_CALL( CMRallocStackArray(cmr, &nodeStacks[1], baseNumRows+1) );
    size_t nodeStackSizes[2];
    CMR_GRAPH_NODE splitNode = -1;
    for (size_t i = 0; i < numNonzeroColumns; ++i)
    {
      CMR_GRAPH_EDGE columnEdge = columnEdges[nonzeroColumns[i]];
      CMR_GRAPH_NODE nodes[2] = { CMRgraphEdgeU(graph, columnEdge), CMRgraphEdgeV(graph, columnEdge) };
      for (int j = 0; j < 2; ++j)
      {
        nodeStackSizes[j] = 0;
        for (CMR_GRAPH_NODE v = nodes[j]; v >= 0; v = nodesParent[v])
          nodeStacks[j][nodeStackSizes[j]++] = v;
      }
      CMRdbgMsg(12, "For nonzero c%ld, paths to root %ld have lengths %ld and %ld.\n", nonzeroColumns[i]+1,
        nodeStacks[0][nodeStackSizes[0]-1], nodeStackSizes[0], nodeStackSizes[1]);

      while (nodeStackSizes[0] > 0 && nodeStackSizes[1] > 0
        && nodeStacks[0][nodeStackSizes[0]-1] == nodeStacks[1][nodeStackSizes[1]-1])
      {
        nodeStackSizes[0]--;
        nodeStackSizes[1]--;
      }
      nodeStackSizes[0]++;

      CMRdbgMsg(12, "For nonzero c%ld, pruned paths have lengths %ld and %ld.\n", nonzeroColumns[i]+1,
        nodeStackSizes[0], nodeStackSizes[1]);

      countCandidates = 0;
      for (int j = 0; j < 2; ++j)
      {
        for (size_t k = 0; k < nodeStackSizes[j]; ++k)
        {
          CMR_GRAPH_NODE v = nodeStacks[j][k];
          if (nodesCandidate[v] == i+1)
          {
            nodesCandidate[v]++;
            countCandidates++;
            splitNode = v;
          }
        }
      }

      CMRdbgMsg(12, "Number of candidate points is %ld.\n", countCandidates);
      if (countCandidates == 0)
        break;
    }

    CMR_CALL( CMRfreeStackArray(cmr, &nodeStacks[1]) );
    CMR_CALL( CMRfreeStackArray(cmr, &nodeStacks[0]) );
    CMR_CALL( CMRfreeStackArray(cmr, &nodesParent) );

    if (countCandidates == 1)
    {
      CMRdbgMsg(12, "Unique candidate node is %ld.\n", splitNode);

      CMRgraphPrint(stdout, graph);
      fflush(stdout);

      for (size_t i = 0; i < numNonzeroColumns; ++i)
      {
        CMR_GRAPH_EDGE columnEdge = columnEdges[nonzeroColumns[i]];
        CMRdbgMsg(14, "1-edge {%ld,%ld}\n", CMRgraphEdgeU(graph, columnEdge), CMRgraphEdgeV(graph, columnEdge));
      }

      size_t numComponents;
      size_t* nodesComponent = NULL;
      CMR_CALL( CMRallocStackArray(cmr, &nodesComponent, CMRgraphNumNodes(graph)) );

      CMR_CALL( findComponents(cmr, graph, columnEdges, splitNode, nodesComponent, &numComponents, nonzeroColumns,
        numNonzeroColumns) );
      for (CMR_GRAPH_NODE v = CMRgraphNodesFirst(graph); CMRgraphNodesValid(graph, v); v = CMRgraphNodesNext(graph, v))
      {
        CMRdbgMsg(14, "Node %ld belongs to component %ld of %ld.\n", v, nodesComponent[v], numComponents);
      }
      assert(numComponents >= 2);

      CMR_GRAPH* auxiliaryGraph = NULL;
      CMR_CALL( CMRgraphCreateEmpty(cmr, &auxiliaryGraph, numComponents, numNonzeroColumns) );
      CMR_GRAPH_NODE* componentAuxiliaryNodes = NULL;
      CMR_CALL( CMRallocStackArray(cmr, &componentAuxiliaryNodes, numComponents) );
      for (size_t c = 0; c < numComponents; ++c)
        CMR_CALL( CMRgraphAddNode(cmr, auxiliaryGraph, &componentAuxiliaryNodes[c]) );

      for (size_t i = 0; i < numNonzeroColumns; ++i)
      {
        CMR_GRAPH_EDGE e = columnEdges[nonzeroColumns[i]];
        size_t components[2] = { nodesComponent[CMRgraphEdgeU(graph, e)], nodesComponent[CMRgraphEdgeV(graph, e)] };
        if (components[0] < SIZE_MAX && components[1] < SIZE_MAX)
        {
          CMR_CALL( CMRgraphAddEdge(cmr, auxiliaryGraph, componentAuxiliaryNodes[components[0]],
            componentAuxiliaryNodes[components[1]], NULL) );
        }
      }

#if defined(CMR_DEBUG)
      CMRdbgMsg(14, "Constructed auxiliary graph.\n");
      CMRgraphPrint(stdout, auxiliaryGraph);
      fflush(stdout);
#endif /* CMR_DEBUG */

      bool isBipartite;
      int* bipartition = NULL;
      CMR_CALL( CMRallocStackArray(cmr, &bipartition, CMRgraphMemNodes(auxiliaryGraph)) );

      CMR_CALL( findBipartition(cmr, auxiliaryGraph, bipartition, &isBipartite) );
      if (isBipartite)
      {
        *pisGraphic = true;

        for (size_t c = 0; c < numComponents; ++c)
        {
          CMRdbgMsg(16, "Component %ld belongs to bipartition %d.\n", c, bipartition[componentAuxiliaryNodes[c]]);
        }

        /* we carry out the re-assignment. */

        CMR_GRAPH_NODE sisterNode;
        CMR_CALL( CMRgraphAddNode(cmr, graph, &sisterNode) );

        /* We mark the 1-edges. */
        bool* edges1 = NULL;
        CMR_CALL( CMRallocStackArray(cmr, &edges1, CMRgraphMemEdges(graph)) );
        for (CMR_GRAPH_ITER iter = CMRgraphEdgesFirst(graph); CMRgraphEdgesValid(graph, iter);
          iter = CMRgraphEdgesNext(graph, iter))
        {
          edges1[CMRgraphEdgesEdge(graph, iter)] = false;
        }
        for (size_t i = 0; i < numNonzeroColumns; ++i)
          edges1[columnEdges[nonzeroColumns[i]]] = true;

        /* We store the incident edges since we change that list. */
        size_t numIncidentEdges = 0;
        CMR_GRAPH_EDGE* incidentEdges = NULL;
        CMR_CALL( CMRallocStackArray(cmr, &incidentEdges, CMRgraphNumNodes(graph)) );
        for (CMR_GRAPH_ITER iter = CMRgraphIncFirst(graph, splitNode); CMRgraphIncValid(graph, iter);
          iter = CMRgraphIncNext(graph, iter))
        {
          incidentEdges[numIncidentEdges++] = CMRgraphIncEdge(graph, iter);
        }

        for (size_t i = 0; i < numIncidentEdges; ++i)
        {
          CMR_GRAPH_EDGE edge = incidentEdges[i];
          CMR_GRAPH_NODE v = CMRgraphEdgeU(graph, edge);
          if (v == splitNode)
            v = CMRgraphEdgeV(graph, edge);
          int side = bipartition[componentAuxiliaryNodes[nodesComponent[v]]];
          CMRdbgMsg(16, "Node %ld of edge {%ld,%ld} belongs to bipartition side %d.\n", v, CMRgraphEdgeU(graph, edge),
            CMRgraphEdgeV(graph, edge), side);

          /* Complement decision for 1-edges. */
          if (edges1[edge])
            side = 1-side;

          if (side)
          {
            /* Reconnect the edge to the sister node. */
            CMR_CALL( CMRgraphDeleteEdge(cmr, graph, edge) );
            CMR_GRAPH_EDGE modifiedEdge;
            CMR_CALL( CMRgraphAddEdge(cmr, graph, v, sisterNode, &modifiedEdge) );
            assert(modifiedEdge == edge);
          }
        }
        
        /* Finally, connect the split node and the sister node. */
        CMR_CALL( CMRgraphAddEdge(cmr, graph, splitNode, sisterNode, &rowEdges[baseNumRows]) );
  
        CMR_CALL( CMRfreeStackArray(cmr, &incidentEdges) );
        CMR_CALL( CMRfreeStackArray(cmr, &edges1) );
      }
      else
      {
        /* Auxiliary graph is not bipartite. */
        *pisGraphic = false;
      }
    
      CMR_CALL( CMRfreeStackArray(cmr, &bipartition) );

      CMR_CALL( CMRfreeStackArray(cmr, &componentAuxiliaryNodes) );
      CMR_CALL( CMRfreeStackArray(cmr, &nodesComponent) );
      CMR_CALL( CMRgraphFree(cmr, &auxiliaryGraph) );
    }
    else
    {
      /* No articular point is part of all fundamental cycles induced by 1-edges. */
      *pisGraphic = false;
    }
  }
  else
  {
    /* No articular point found. */
    *pisGraphic = false;
  }

  CMR_CALL( CMRfreeStackArray(cmr, &nodesCandidate) );

  return CMR_OKAY;
}

/**
 * \brief Find element in submatrix parallel to vector.
 */

static
CMR_ELEMENT findParallel(
  CMR_CHRMAT* matrix,
  size_t row,
  size_t numRows,
  size_t numColumns,
  long long* rowHashValues,
  long long* hashVector
)
{
  assert(matrix);
  assert(rowHashValues);

  long long hashValue = 0;
  size_t first = matrix->rowSlice[row];
  size_t beyond = matrix->rowSlice[row + 1];
  size_t countNonzeros = 0;
  for (size_t e = first; e < beyond; ++e)
  {
    size_t column = matrix->entryColumns[e];
    if (column < numColumns)
    {
      hashValue = projectSignedHash(hashValue + hashVector[column]);
      ++countNonzeros;
    }
    else
      break;
  }

  assert(countNonzeros >= 1);
  if (countNonzeros == 1)
    return CMRcolumnToElement(matrix->entryColumns[first]);

  for (size_t row2 = 0; row2 < numRows; ++row2)
  {
    if (rowHashValues[row2] != hashValue)
      continue;

    size_t first2 = matrix->rowSlice[row2];
    size_t beyond2 = matrix->rowSlice[row2 + 1];
    size_t e = first;
    size_t e2 = first2;
    bool isParallel = true;
    while (e < beyond && e2 < beyond2)
    {
      size_t column = matrix->entryColumns[e];
      size_t column2 = matrix->entryColumns[e2];
      if (column >= numColumns && column2 >= numColumns)
        break;
      if (column != column2)
      {
        isParallel = false;
        break;
      }
      ++e;
      ++e2;
    }
    if (isParallel)
      return CMRrowToElement(row2);
  }

  return 0;
}


/**
 * \brief Creates a hash vector to speed-up recognition of parallel vectors.
 */

static
CMR_ERROR createHashVector(
  CMR* cmr,                 /**< \ref CMR environment. */
  long long** phashVector,  /**< Pointer for storing the hash vector. */
  size_t size               /**< Size of hash vector. */
)
{
  assert(cmr);

  CMR_CALL( CMRallocStackArray(cmr, phashVector, size) );
  long long* hashVector = *phashVector;
  size_t h = 1;
  for (size_t e = 0; e < size; ++e)
  {
    hashVector[e] = h;
    h = projectSignedHash(3 * h);
  }

  return CMR_OKAY;
}

/**
 * \brief Update the hash values of rows/column of submatrix that is grown by a number of rows.
 */

static
CMR_ERROR updateHashValues(
  CMR_CHRMAT* matrix,         /**< Matrix. */
  long long* majorHashValues, /**< Map for hash values of major indices. */
  long long* minorHashValues, /**< Map for hash values of minor indices. */
  long long* hashVector,      /**< Hash vector. */
  size_t majorFirst,          /**< First new major index. */
  size_t majorBeyond,         /**< Last new major index plus 1. */
  size_t minorSize            /**< Number of minor indices in submatrix. */
)
{
  assert(matrix);
  assert(hashVector);

  for (size_t major = majorFirst; major < majorBeyond; ++major)
  {
    size_t first = matrix->rowSlice[major];
    size_t beyond = matrix->rowSlice[major + 1];
    for (size_t e = first; e < beyond; ++e)
    {
      size_t minor = matrix->entryColumns[e];
      if (minor < minorSize)
      {
        majorHashValues[major] = projectSignedHash(majorHashValues[major] + hashVector[minor]);
        minorHashValues[minor] = projectSignedHash(minorHashValues[minor] + hashVector[major]);
      }
      else
        break;
    }
  }

  return CMR_OKAY;
}


/**
 * \brief Returns \c true if two edges \p e and \p f are adjacent.
 */

static
bool checkEdgesAdjacent(
  CMR_GRAPH* graph,         /**< Graph. */
  CMR_GRAPH_EDGE e,         /**< First edge. */
  CMR_GRAPH_EDGE f,         /**< Second edge. */
  CMR_GRAPH_NODE* pcommon,  /**< Pointer for storing the common endnode. */
  CMR_GRAPH_NODE* peOther,  /**< Pointer for storing the endnode of \p e that is not common. */
  CMR_GRAPH_NODE* pfOther   /**< Pointer for storing the endnode of \p f that is not common. */
)
{
  assert(graph);
  assert(pcommon);
  assert(peOther);
  assert(pfOther);

  CMR_GRAPH_NODE eNodes[2] = { CMRgraphEdgeU(graph, e), CMRgraphEdgeV(graph, e) };
  CMR_GRAPH_NODE fNodes[2] = { CMRgraphEdgeU(graph, f), CMRgraphEdgeV(graph, f) };
  for (int i = 0; i < 2; ++i)
  {
    for (int j = 0; j < 2; ++j)
    {
      if (eNodes[i] == fNodes[j])
      {
        *pcommon = eNodes[i];
        *peOther = eNodes[1-i];
        *pfOther = fNodes[1-j];
        return true;
      }
    }
  }
  return false;
}

/**
 * \brief Extends \p graph for a submatrix augmented by 1 row and 1 column.
 */

static
CMR_ERROR addToGraph1Row1Column(
  CMR* cmr,                     /**< \ref CMR environment. */
  CMR_GRAPH* graph,             /**< Empty graph to be filled. */
  CMR_GRAPH_EDGE* rowEdges,     /**< Array to be filled with map from rows to edges. */
  CMR_GRAPH_EDGE* columnEdges,  /**< Array to be filled with map from columns to edges. */
  size_t baseNumRows,           /**< Number of rows already processed. */
  size_t baseNumColumns,        /**< Number of columns already processed. */    
  CMR_ELEMENT rowParallel,      /**< Element to which the row is parallel. */
  CMR_ELEMENT columnParallel,   /**< Element to which the column is parallel. */
  bool* pisGraphic              /**< Pointer for storing whether this extension was graphic. */
)
{
  assert(cmr);
  assert(graph);
  assert(baseNumRows >= 3);
  assert(baseNumColumns >= 3);
  assert(rowEdges);
  assert(columnEdges);
  assert(CMRelementIsValid(rowParallel));
  assert(CMRelementIsValid(columnParallel));

  CMR_GRAPH_EDGE rowEdge, columnEdge;
  if (CMRelementIsRow(rowParallel))
    rowEdge = rowEdges[CMRelementToRowIndex(rowParallel)];
  else
    rowEdge = columnEdges[CMRelementToColumnIndex(rowParallel)];
  CMRdbgMsg(12, "Row edge is {%ld,%ld}.\n", CMRgraphEdgeU(graph, rowEdge), CMRgraphEdgeV(graph, rowEdge));
  if (CMRelementIsRow(columnParallel))
    columnEdge = rowEdges[CMRelementToRowIndex(columnParallel)];
  else
    columnEdge = columnEdges[CMRelementToColumnIndex(columnParallel)];
  CMRdbgMsg(12, "Column edge is {%ld,%ld}.\n", CMRgraphEdgeU(graph, columnEdge), CMRgraphEdgeV(graph, columnEdge));

  CMR_GRAPH_NODE common, rowOther, columnOther;
  if (checkEdgesAdjacent(graph, rowEdge, columnEdge, &common, &rowOther, &columnOther))
  {
    *pisGraphic = true;
    CMR_GRAPH_NODE rowSplit;
    CMR_CALL( CMRgraphAddNode(cmr, graph, &rowSplit) );
    CMR_CALL( CMRgraphDeleteEdge(cmr, graph, rowEdge) );
    CMR_GRAPH_EDGE modifiedRowEdge, newRowEdge, newColumnEdge;
    CMR_CALL( CMRgraphAddEdge(cmr, graph, rowOther, rowSplit, &modifiedRowEdge) );
    assert(modifiedRowEdge == rowEdge);
    CMR_CALL( CMRgraphAddEdge(cmr, graph, rowSplit, common, &newRowEdge) );
    rowEdges[baseNumRows] = newRowEdge;
    CMR_CALL( CMRgraphAddEdge(cmr, graph, rowSplit, columnOther, &newColumnEdge) );
    columnEdges[baseNumColumns] = newColumnEdge;
  }
  else
    *pisGraphic = false;

  return CMR_OKAY;
}

/**
 * \brief Extends \p graph for a submatrix augmented by 2 rows and 1 column.
 */

static
CMR_ERROR addToGraph2Rows1Column(
  CMR* cmr,                     /**< \ref CMR environment. */
  CMR_GRAPH* graph,             /**< Empty graph to be filled. */
  CMR_GRAPH_EDGE* rowEdges,     /**< Array to be filled with map from rows to edges. */
  CMR_GRAPH_EDGE* columnEdges,  /**< Array to be filled with map from columns to edges. */
  size_t baseNumRows,           /**< Number of rows already processed. */
  size_t baseNumColumns,        /**< Number of columns already processed. */    
  CMR_ELEMENT row1Parallel,     /**< Element to which row1 is parallel. */
  CMR_ELEMENT row2Parallel,     /**< Element to which row2 is parallel. */
  bool* pisGraphic              /**< Pointer for storing whether this extension was graphic. */
)
{
  assert(cmr);
  assert(graph);
  assert(baseNumRows >= 3);
  assert(baseNumColumns >= 3);
  assert(rowEdges);
  assert(columnEdges);
  assert(CMRelementIsValid(row1Parallel));
  assert(CMRelementIsValid(row2Parallel));

  CMR_GRAPH_EDGE row1Edge, row2Edge;
  if (CMRelementIsRow(row1Parallel))
    row1Edge = rowEdges[CMRelementToRowIndex(row1Parallel)];
  else
    row1Edge = columnEdges[CMRelementToColumnIndex(row1Parallel)];
  CMRdbgMsg(12, "Row1's edge is {%ld,%ld}.\n", CMRgraphEdgeU(graph, row1Edge), CMRgraphEdgeV(graph, row1Edge));
  if (CMRelementIsRow(row2Parallel))
    row2Edge = rowEdges[CMRelementToRowIndex(row2Parallel)];
  else
    row2Edge = columnEdges[CMRelementToColumnIndex(row2Parallel)];
  CMRdbgMsg(12, "Row2's edge is {%ld,%ld}.\n", CMRgraphEdgeU(graph, row2Edge), CMRgraphEdgeV(graph, row2Edge));

  CMR_GRAPH_NODE common, other1, other2;
  if (checkEdgesAdjacent(graph, row1Edge, row2Edge, &common, &other1, &other2))
  {
    *pisGraphic = true;

    CMR_GRAPH_NODE row1Split;
    CMR_CALL( CMRgraphAddNode(cmr, graph, &row1Split) );
    CMR_CALL( CMRgraphDeleteEdge(cmr, graph, row1Edge) );
    CMR_GRAPH_EDGE modifiedRow1Edge;
    CMR_CALL( CMRgraphAddEdge(cmr, graph, other1, row1Split, &modifiedRow1Edge) );
    CMR_CALL( CMRgraphAddEdge(cmr, graph, row1Split, common, &rowEdges[baseNumRows]) );
    assert(modifiedRow1Edge == row1Edge);

    CMRdbgMsg(12, "Row1's edge {%ld,%ld} is subdivided with new node %ld.\n", other1, common, row1Split);

    CMR_GRAPH_NODE row2Split;
    CMR_CALL( CMRgraphAddNode(cmr, graph, &row2Split) );
    CMR_CALL( CMRgraphDeleteEdge(cmr, graph, row2Edge) );
    CMR_GRAPH_EDGE modifiedRow2Edge;
    CMR_CALL( CMRgraphAddEdge(cmr, graph, other2, row2Split, &modifiedRow2Edge) );
    CMR_CALL( CMRgraphAddEdge(cmr, graph, row2Split, common, &rowEdges[baseNumRows+1]) );
    assert(modifiedRow2Edge == row2Edge);

    CMRdbgMsg(12, "Row2's edge {%ld,%ld} is subdivided with new node %ld.\n", other2, common, row2Split);

    CMR_CALL( CMRgraphAddEdge(cmr, graph, row1Split, row2Split, &columnEdges[baseNumColumns]) );
  }
  else
    *pisGraphic = false;

  return CMR_OKAY;
}

/**
 * \brief Extends \p graph for a submatrix augmented by 1 row and 2 columns.
 */

static
CMR_ERROR addToGraph1Row2Columns(
  CMR* cmr,                     /**< \ref CMR environment. */
  CMR_GRAPH* graph,             /**< Empty graph to be filled. */
  CMR_GRAPH_EDGE* rowEdges,     /**< Array to be filled with map from rows to edges. */
  CMR_GRAPH_EDGE* columnEdges,  /**< Array to be filled with map from columns to edges. */
  size_t baseNumRows,           /**< Number of rows already processed. */
  size_t baseNumColumns,        /**< Number of columns already processed. */    
  CMR_ELEMENT column1Parallel,  /**< Element to which column1 is parallel. */
  CMR_ELEMENT column2Parallel,  /**< Element to which column2 is parallel. */
  bool* pisGraphic              /**< Pointer for storing whether this extension was graphic. */
)
{
  assert(cmr);
  assert(graph);
  assert(baseNumRows >= 3);
  assert(baseNumColumns >= 3);
  assert(rowEdges);
  assert(columnEdges);
  assert(CMRelementIsValid(column1Parallel));
  assert(CMRelementIsValid(column2Parallel));

  CMR_GRAPH_EDGE column1Edge, column2Edge;
  if (CMRelementIsRow(column1Parallel))
    column1Edge = rowEdges[CMRelementToRowIndex(column1Parallel)];
  else
    column1Edge = columnEdges[CMRelementToColumnIndex(column1Parallel)];
  CMRdbgMsg(12, "Column1's edge is {%ld,%ld}.\n", CMRgraphEdgeU(graph, column1Edge), CMRgraphEdgeV(graph, column1Edge));
  if (CMRelementIsRow(column2Parallel))
    column2Edge = rowEdges[CMRelementToRowIndex(column2Parallel)];
  else
    column2Edge = columnEdges[CMRelementToColumnIndex(column2Parallel)];
  CMRdbgMsg(12, "Column2's edge is {%ld,%ld}.\n", CMRgraphEdgeU(graph, column2Edge), CMRgraphEdgeV(graph, column2Edge));

  CMR_GRAPH_NODE common, other1, other2;
  if (checkEdgesAdjacent(graph, column1Edge, column2Edge, &common, &other1, &other2))
  {
    *pisGraphic = true;
    
    CMR_GRAPH_NODE newNode;
    CMR_CALL( CMRgraphAddNode(cmr, graph, &newNode) );
    CMR_CALL( CMRgraphAddEdge(cmr, graph, other1, newNode, &columnEdges[baseNumColumns]) );
    CMR_CALL( CMRgraphAddEdge(cmr, graph, other2, newNode, &columnEdges[baseNumColumns + 1]) );
    CMR_CALL( CMRgraphAddEdge(cmr, graph, common, newNode, &rowEdges[baseNumRows]) );
  }
  else
    *pisGraphic = false;

  return CMR_OKAY;
}

/**
 * \brief Extends \p graph for a submatrix augmented by 1 column.
 */

static
CMR_ERROR addToGraph1Column(
  CMR* cmr,                     /**< \ref CMR environment. */
  CMR_GRAPH* graph,             /**< Empty graph to be filled. */
  CMR_GRAPH_EDGE* rowEdges,     /**< Array to be filled with map from rows to edges. */
  CMR_GRAPH_EDGE* columnEdges,  /**< Array to be filled with map from columns to edges. */
  size_t baseNumRows,           /**< Number of rows already processed. */
  size_t baseNumColumns,        /**< Number of columns already processed. */
  size_t* nonzeroRows,          /**< Array with rows containing a nonzero. */
  size_t numNonzeroRows,        /**< Length of \p nonzeroRows. */
  bool* pisGraphic              /**< Pointer for storing whether this extension was graphic. */
)
{
  assert(cmr);
  assert(graph);
  assert(baseNumRows >= 3);
  assert(baseNumColumns >= 3);
  assert(rowEdges);
  assert(columnEdges);
  assert(numNonzeroRows <= baseNumRows);

  size_t* nodeDegrees = NULL;
  CMR_CALL( CMRallocStackArray(cmr, &nodeDegrees, CMRgraphMemNodes(graph)) );
  for (CMR_GRAPH_NODE v = CMRgraphNodesFirst(graph); CMRgraphNodesValid(graph, v); v = CMRgraphNodesNext(graph, v))
    nodeDegrees[v] = 0;

  size_t countLeaves = 0;
  for (size_t i = 0; i < numNonzeroRows; ++i)
  {
    CMR_GRAPH_EDGE e = rowEdges[nonzeroRows[i]];
    size_t deg = ++nodeDegrees[CMRgraphEdgeU(graph, e)];
    if (deg == 1)
      ++countLeaves;
    else if (deg == 2)
      --countLeaves;
    deg = ++nodeDegrees[CMRgraphEdgeV(graph, e)];
    if (deg == 1)
      ++countLeaves;
    else if (deg == 2)
      --countLeaves;
  }

  *pisGraphic = countLeaves == 2;

  if (*pisGraphic)
  {
    CMR_GRAPH_NODE nodes[2] = { -1, -1 };
    for (CMR_GRAPH_NODE v = CMRgraphNodesFirst(graph); CMRgraphNodesValid(graph, v); v = CMRgraphNodesNext(graph, v))
    {
      if (nodeDegrees[v] == 1)
      {
        if (nodes[0] < 0)
          nodes[0] = v;
        else
          nodes[1] = v;
      }
    }
    CMR_CALL( CMRgraphAddEdge(cmr, graph, nodes[0], nodes[1], &columnEdges[baseNumColumns]) );
  }

  CMR_CALL( CMRfreeStackArray(cmr, &nodeDegrees) );

  return CMR_OKAY;
}

/**
 * \brief Creates the wheel graph for a wheel submatrix.
 */

static
CMR_ERROR createWheel(
  CMR* cmr,                   /**< \ref CMR environment. */
  CMR_GRAPH* graph,           /**< Empty graph to be filled. */
  CMR_CHRMAT* matrix,         /**< Matrix. */
  CMR_CHRMAT* transpose,      /**< Transpose of \p matrix. */
  size_t wheelSize,           /**< Size of wheel. */
  CMR_GRAPH_EDGE* rowEdges,   /**< Array to be filled with map from rows to edges. */
  CMR_GRAPH_EDGE* columnEdges /**< Array to be filled with map from columns to edges. */
)
{
  assert(graph);
  assert(matrix);
  assert(transpose);
  assert(wheelSize <= matrix->numRows);
  assert(wheelSize <= matrix->numColumns);
  assert(rowEdges);
  assert(columnEdges);

  CMRdbgMsg(8, "Creating wheel graph W_%ld for first minor.\n", wheelSize);

  /* Check which row contains 3 indices (if any). */
  size_t rowWithThree = SIZE_MAX;
  for (size_t row = 0; row < wheelSize; ++row)
  {
    size_t count = 0;
    size_t first = matrix->rowSlice[row];
    size_t beyond = matrix->rowSlice[row + 1];
    for (size_t e = first; e < beyond; ++e)
    {
      if (matrix->entryColumns[e] < wheelSize)
        ++count;
      else
        break;
    }
    assert(count == 2 || count == 3);
    if (count == 3)
    {
      assert(rowWithThree == SIZE_MAX);
      rowWithThree = row;
    }
  }

  /* Check which column contains 3 indices (if any). */
  size_t columnWithThree = SIZE_MAX;
  for (size_t column = 0; column < wheelSize; ++column)
  {
    size_t count = 0;
    size_t first = transpose->rowSlice[column];
    size_t beyond = transpose->rowSlice[column + 1];
    for (size_t e = first; e < beyond; ++e)
    {
      if (transpose->entryColumns[e] < wheelSize)
        ++count;
      else
        break;
    }
    assert(count == 2 || count == 3);
    if (count == 3)
    {
      assert(columnWithThree == SIZE_MAX);
      columnWithThree = column;
    }
  }

  assert((rowWithThree == SIZE_MAX && columnWithThree == SIZE_MAX)
    || (rowWithThree < SIZE_MAX && columnWithThree < SIZE_MAX));

  CMR_GRAPH_NODE centerNode, firstRimNode;
  CMR_CALL( CMRgraphAddNode(cmr, graph, &centerNode) );
  CMR_CALL( CMRgraphAddNode(cmr, graph, &firstRimNode) );
  CMR_GRAPH_NODE lastRimNode = firstRimNode;

  size_t lastRow = 0;
  size_t lastColumn = matrix->entryColumns[matrix->rowSlice[0]];
  size_t nextRow = SIZE_MAX;
  size_t nextColumn = SIZE_MAX;
  while (nextRow)
  {
    size_t e = matrix->rowSlice[lastRow];
    if (lastRow == rowWithThree)
    {
      nextColumn = matrix->entryColumns[e];
      if (nextColumn == lastColumn || nextColumn == columnWithThree)
        nextColumn = matrix->entryColumns[e + 1];
      if (nextColumn == lastColumn || nextColumn == columnWithThree)
        nextColumn = matrix->entryColumns[e + 2];
    }
    else
    {
      nextColumn = matrix->entryColumns[e];
      if (nextColumn == lastColumn)
        nextColumn = matrix->entryColumns[e + 1];
    }

    e = transpose->rowSlice[nextColumn];
    if (nextColumn == columnWithThree)
    {
      nextRow = transpose->entryColumns[e];
      if (nextRow == lastRow || nextRow == rowWithThree)
        nextRow = transpose->entryColumns[e + 1];
      if (nextRow == lastRow || nextRow == rowWithThree)
        nextRow = transpose->entryColumns[e + 2];
    }
    else
    {
      nextRow = transpose->entryColumns[e];
      if (nextRow == lastRow)
        nextRow = transpose->entryColumns[e + 1];
    }

    CMRdbgMsg(10, "next column = %ld, next row = %ld\n", nextColumn, nextRow);

    CMR_GRAPH_NODE nextRimNode;
    CMR_GRAPH_EDGE rimEdge;
    CMR_GRAPH_EDGE spokeEdge;
    if (nextRow == 0)
      nextRimNode = firstRimNode;
    else
      CMR_CALL( CMRgraphAddNode(cmr, graph, &nextRimNode) );
    CMR_CALL( CMRgraphAddEdge(cmr, graph, lastRimNode, nextRimNode, &rimEdge) );

    CMRdbgMsg(10, "Added rim {%ld,%ld} for column %ld.\n", lastRimNode, nextRimNode, lastColumn);
    
    CMR_CALL( CMRgraphAddEdge(cmr, graph, centerNode, nextRimNode, &spokeEdge) );

    if (rowWithThree < SIZE_MAX && lastRow != rowWithThree && nextRow != rowWithThree)
    {
      columnEdges[lastColumn] = spokeEdge;
      rowEdges[lastRow] = rimEdge;
    }
    else
    {
      columnEdges[lastColumn] = rimEdge;
      rowEdges[lastRow] = spokeEdge;
    }

    CMRdbgMsg(10, "Added spoke {%ld,%ld} for row %ld.\n", centerNode, nextRimNode, lastRow);

    lastRimNode = nextRimNode;
    lastRow = nextRow;
    lastColumn = nextColumn;
  }

  return CMR_OKAY;
}


CMR_ERROR CMRregularSequenceGraphic(CMR* cmr, CMR_CHRMAT* matrix, CMR_CHRMAT* transpose, CMR_ELEMENT* rowElements,
  CMR_ELEMENT* columnElements, size_t lengthSequence, size_t* sequenceNumRows, size_t* sequenceNumColumns,
  size_t* plastGraphicMinor, CMR_GRAPH** pgraph, CMR_ELEMENT** pedgeElements)
{
  assert(cmr);
  assert(matrix);
  assert(transpose);
  assert(rowElements);
  assert(columnElements);
  assert(sequenceNumRows);
  assert(sequenceNumColumns);
  assert(plastGraphicMinor);
  assert(pgraph);
  assert(!*pgraph);
  assert(pedgeElements);
  assert(!*pedgeElements);

  CMRdbgMsg(8, "Testing sequence for (co)graphicness.\n");

  CMR_CALL( CMRgraphCreateEmpty(cmr, pgraph, matrix->numRows, matrix->numRows + matrix->numColumns) );
  CMR_GRAPH* graph = *pgraph;

  long long* hashVector = NULL;
  CMR_CALL( createHashVector(cmr, &hashVector,
    matrix->numRows > matrix->numColumns ? matrix->numRows : matrix->numColumns) );
  CMR_GRAPH_EDGE* rowEdges = NULL;
  CMR_CALL( CMRallocStackArray(cmr, &rowEdges, matrix->numRows) );
  CMR_GRAPH_EDGE* columnEdges = NULL;
  CMR_CALL( CMRallocStackArray(cmr, &columnEdges, matrix->numColumns) );
  long long* rowHashValues = NULL;
  CMR_CALL( CMRallocStackArray(cmr, &rowHashValues, matrix->numRows) );
  for (size_t row = 0; row < matrix->numRows; ++row)
    rowHashValues[row] = 0;
  long long* columnHashValues = NULL;
  CMR_CALL( CMRallocStackArray(cmr, &columnHashValues, matrix->numColumns) );
  for (size_t column = 0; column < matrix->numColumns; ++column)
    columnHashValues[column] = 0;

  /* Create graph for first minor. */

  assert(sequenceNumRows[0] == sequenceNumColumns[0]);
  CMR_CALL( createWheel(cmr, graph, matrix, transpose, sequenceNumRows[0], rowEdges, columnEdges) );
  *plastGraphicMinor = 0;

  CMR_CALL( updateHashValues(matrix, rowHashValues, columnHashValues, hashVector, 0, sequenceNumRows[0],
    sequenceNumColumns[0]) );

  for (size_t extension = 1; extension < lengthSequence; ++extension)
  { 
    size_t newRows = sequenceNumRows[extension] - sequenceNumRows[extension-1];
    size_t newColumns = sequenceNumColumns[extension] - sequenceNumColumns[extension-1];

    CMRdbgMsg(10, "Processing extension step %ld with %ld new rows and %ld new columns.\n", extension, newRows,
      newColumns);

    bool isGraphic;
    if (newRows == 1 && newColumns == 1)
    {
      CMR_ELEMENT rowParallel = findParallel(matrix, sequenceNumRows[extension-1], sequenceNumRows[extension-1],
        sequenceNumColumns[extension-1], rowHashValues, hashVector);
      CMR_ELEMENT columnParallel = CMRelementTranspose(findParallel(transpose, sequenceNumColumns[extension-1],
        sequenceNumColumns[extension-1], sequenceNumRows[extension-1], columnHashValues, hashVector));

      CMRdbgMsg(10, "The new row is parallel to %s", CMRelementString(rowParallel, 0));
      CMRdbgMsg(0, " and the new column is parallel to %s.\n", CMRelementString(columnParallel, 0));

      CMR_CALL( addToGraph1Row1Column(cmr, graph, rowEdges, columnEdges, sequenceNumRows[extension-1],
        sequenceNumColumns[extension-1], rowParallel, columnParallel, &isGraphic) );
    }
    else if (newRows == 2 && newColumns == 1)
    {
      CMR_ELEMENT row1Parallel = findParallel(matrix, sequenceNumRows[extension-1], sequenceNumRows[extension-1],
        sequenceNumColumns[extension-1], rowHashValues, hashVector);
      CMR_ELEMENT row2Parallel = findParallel(matrix, sequenceNumRows[extension-1] + 1, sequenceNumRows[extension-1],
        sequenceNumColumns[extension-1], rowHashValues, hashVector);

      CMRdbgMsg(10, "Row 1 is parallel to %s", CMRelementString(row1Parallel, 0));
      CMRdbgMsg(0, " and row 2 is parallel to %s.\n", CMRelementString(row2Parallel, 0));

      CMR_CALL( addToGraph2Rows1Column(cmr, graph, rowEdges, columnEdges, sequenceNumRows[extension-1],
        sequenceNumColumns[extension-1], row1Parallel, row2Parallel, &isGraphic) );
    }
    else if (newRows == 1 && newColumns == 2)
    {
      CMR_ELEMENT column1Parallel = CMRelementTranspose(findParallel(transpose, sequenceNumColumns[extension-1],
        sequenceNumColumns[extension-1], sequenceNumRows[extension-1], columnHashValues, hashVector));
      CMR_ELEMENT column2Parallel = CMRelementTranspose(findParallel(transpose, sequenceNumColumns[extension-1] + 1,
        sequenceNumColumns[extension-1], sequenceNumRows[extension-1], columnHashValues, hashVector));

      CMRdbgMsg(10, "Column 1 is parallel to %s", CMRelementString(column1Parallel, 0));
      CMRdbgMsg(0, " and column 2 is parallel to %s.\n", CMRelementString(column2Parallel, 0));

      CMR_CALL( addToGraph1Row2Columns(cmr, graph, rowEdges, columnEdges, sequenceNumRows[extension-1],
        sequenceNumColumns[extension-1], column1Parallel, column2Parallel, &isGraphic) );
    }
    else if (newRows == 0 && newColumns == 1)
    {
      size_t first = transpose->rowSlice[sequenceNumColumns[extension-1]];
      size_t beyond = transpose->rowSlice[sequenceNumColumns[extension-1] + 1];
      for (size_t e = first; e < beyond; ++e)
      {
        if (transpose->entryColumns[e] >= sequenceNumRows[extension-1])
          beyond = e;
      }
      CMR_CALL( addToGraph1Column(cmr, graph, rowEdges, columnEdges, sequenceNumRows[extension-1],
        sequenceNumColumns[extension-1], &transpose->entryColumns[first], beyond-first,
        &isGraphic) );
    }
    else
    {
      assert(newRows == 1 && newColumns == 0);

      size_t first = matrix->rowSlice[sequenceNumRows[extension-1]];
      size_t beyond = matrix->rowSlice[sequenceNumRows[extension-1] + 1];
      for (size_t e = first; e < beyond; ++e)
      {
        if (matrix->entryColumns[e] >= sequenceNumColumns[extension-1])
          beyond = e;
      }
      CMR_CALL( addToGraph1Row(cmr, graph, rowEdges, columnEdges, sequenceNumRows[extension-1],
        sequenceNumColumns[extension-1], &matrix->entryColumns[first], beyond-first,
        &isGraphic) );
    }

    if (isGraphic)
      *plastGraphicMinor = extension;
    else
      break;

    CMR_CALL( updateHashValues(matrix, rowHashValues, columnHashValues, hashVector, sequenceNumRows[extension-1],
      sequenceNumRows[extension], sequenceNumColumns[extension - 1]) );
    CMR_CALL( updateHashValues(transpose, columnHashValues, rowHashValues, hashVector, sequenceNumColumns[extension-1],
      sequenceNumColumns[extension], sequenceNumRows[extension]) );
  }

  if (*plastGraphicMinor == lengthSequence - 1)
  {
    CMR_CALL( CMRallocBlockArray(cmr, pedgeElements, matrix->numRows + matrix->numColumns) );
    CMR_ELEMENT* edgeElements = *pedgeElements;
    for (size_t e = 0; e < matrix->numRows + matrix->numColumns; ++e)
      edgeElements[e] = 0;

    for (size_t row = 0; row < matrix->numRows; ++row)
      edgeElements[rowEdges[row]] = CMRrowToElement(row);
    for (size_t column = 0; column < matrix->numColumns; ++column)
      edgeElements[columnEdges[column]] = CMRcolumnToElement(column);
  }
  else
  {
    CMR_CALL( CMRgraphFree(cmr, pgraph) );
  }

  CMR_CALL( CMRfreeStackArray(cmr, &columnHashValues) );
  CMR_CALL( CMRfreeStackArray(cmr, &rowHashValues) );
  CMR_CALL( CMRfreeStackArray(cmr, &columnEdges) );
  CMR_CALL( CMRfreeStackArray(cmr, &rowEdges) );
  CMR_CALL( CMRfreeStackArray(cmr, &hashVector) );

  return CMR_OKAY;
}

CMR_ERROR CMRregularTestGraphic(CMR* cmr, CMR_CHRMAT** pmatrix, CMR_CHRMAT** ptranspose, bool ternary, bool* pisGraphic,
  CMR_GRAPH** pgraph, CMR_GRAPH_EDGE** pforest, CMR_GRAPH_EDGE** pcoforest, bool** parcsReversed,
  CMR_SUBMAT** psubmatrix)
{
  assert(cmr);
  assert(pmatrix);
  assert(ptranspose);
  assert(pisGraphic);

  CMR_CHRMAT* matrix = *pmatrix;
  CMR_CHRMAT* transpose = *ptranspose;

  assert(matrix || transpose);

  if (!transpose)
  {
    CMR_CALL( CMRchrmatTranspose(cmr, matrix, ptranspose) );
    transpose = *ptranspose;
  }

  if (ternary)
  {
    CMR_CALL( CMRtestConetworkMatrix(cmr, transpose, pisGraphic, pgraph, pforest, pcoforest, parcsReversed,
      psubmatrix) );
  }
  else
  {
    CMR_CALL( CMRtestCographicMatrix(cmr, transpose, pisGraphic, pgraph, pforest, pcoforest, psubmatrix) );
  }

  return CMR_OKAY;
}
