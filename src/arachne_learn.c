#include "../include/arachne.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

int arachne_learn(ArachneGraph* graph, uint64_t node_id,
                  const char* event, double impact) {
    if (!graph || !event) return -1;
    
    pthread_rwlock_wrlock(&graph->lock);
    
    ArachneNode* node = arachne_get_node(graph, node_id);
    if (!node) {
        pthread_rwlock_unlock(&graph->lock);
        return -1;
    }
    
    node->value += impact;
    node->updated_at = time(NULL);
    
    uint64_t edge_id = node->first_edge;
    while (edge_id != 0) {
        ArachneEdge* edge = &graph->edges[edge_id];
        
        if (strstr(edge->relation, event) || strstr(edge->context, event)) {
            edge->probability = fmin(1.0, edge->probability + impact * 0.1);
        }
        
        edge->updated_at = time(NULL);
        edge_id = edge->next_edge;
    }
    
    graph->is_dirty = true;
    pthread_rwlock_unlock(&graph->lock);
    
    return 0;
}

int arachne_adapt_weights(ArachneGraph* graph) {
    if (!graph) return -1;
    
    pthread_rwlock_wrlock(&graph->lock);
    
    for (uint64_t i = 0; i < graph->header->edge_count; i++) {
        ArachneEdge* edge = &graph->edges[i];
        if (edge->from_id == 0 && edge->to_id == 0) continue;
        
        if (edge->occurrences > 0) {
            double success_rate = (double)edge->successes / edge->occurrences;
            edge->weight = 0.5 + success_rate * 0.5;
            
            edge->probability = edge->probability * 0.9 + success_rate * 0.1;
            if (edge->probability > 1.0) edge->probability = 1.0;
            if (edge->probability < 0.0) edge->probability = 0.0;
        }
    }
    
    graph->is_dirty = true;
    pthread_rwlock_unlock(&graph->lock);
    
    return 0;
}

int arachne_detect_anomalies(ArachneGraph* graph, uint64_t** anomalies, int* count) {
    if (!graph || !anomalies || !count) return -1;
    
    pthread_rwlock_rdlock(&graph->lock);
    
    *anomalies = malloc(graph->header->node_count * sizeof(uint64_t));
    int found = 0;
    
    for (uint64_t i = 0; i < graph->header->node_count; i++) {
        ArachneNode* node = &graph->nodes[i];
        if (node->state == 0) continue;
        
        if (fabs(node->value) > 1000) {
            (*anomalies)[found++] = i;
            continue;
        }
        
        uint64_t edge_id = node->first_edge;
        int edge_count = 0;
        double total_prob = 0;
        
        while (edge_id != 0) {
            ArachneEdge* edge = &graph->edges[edge_id];
            edge_count++;
            total_prob += edge->probability;
            edge_id = edge->next_edge;
        }
        
        if (edge_count > 0) {
            double avg_prob = total_prob / edge_count;
            if (avg_prob < 0.1 && edge_count > 5) {
                (*anomalies)[found++] = i;
            }
        }
    }
    
    pthread_rwlock_unlock(&graph->lock);
    *count = found;
    return 0;
}

double arachne_calculate_accuracy(ArachneGraph* graph) {
    if (!graph || graph->header->total_predictions == 0) return 0.0;
    
    pthread_rwlock_rdlock(&graph->lock);
    double accuracy = (double)graph->header->successful_predictions / 
                     graph->header->total_predictions;
    pthread_rwlock_unlock(&graph->lock);
    
    return accuracy;
}
