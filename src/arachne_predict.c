#include "../include/arachne.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

// ============================================================
// PREDICT 
// ============================================================
ArachnePrediction* arachne_predict(ArachneGraph* graph, ArachneContext* ctx) {
    if (!graph || !ctx) {
        fprintf(stderr, "Error: graph or context is NULL\n");
        return NULL;
    }

    pthread_rwlock_rdlock(&graph->lock);

    uint64_t current = ctx->start_node;
    int depth = 0;
    ArachnePrediction* results = NULL;
    

    if (current >= graph->header->node_count) {
        fprintf(stderr, "Error: node %lu out of range\n", current);
        pthread_rwlock_unlock(&graph->lock);
        return NULL;
    }
    
    ArachneNode* start_node = arachne_get_node(graph, current);
    if (!start_node) {
        fprintf(stderr, "Error: node %lu not found\n", current);
        pthread_rwlock_unlock(&graph->lock);
        return NULL;
    }


    uint64_t visited[MAX_QUERY_DEPTH + 1];
    int visited_count = 0;
    visited[visited_count++] = current;

    while (depth < ctx->max_depth) {
        ArachneNode* node = arachne_get_node(graph, current);
        if (!node) break;

        uint64_t edge_id = node->first_edge;
        uint64_t best_next = 0;
        double best_prob = 0;
        uint64_t best_edge = 0;


        while (edge_id != 0) {
            if (edge_id >= graph->header->edge_count) break;
            
            ArachneEdge* edge = &graph->edges[edge_id];


            if (edge->to_id >= graph->header->node_count) {
                edge_id = edge->next_edge;
                continue;
            }


            int already_visited = 0;
            for (int i = 0; i < visited_count; i++) {
                if (visited[i] == edge->to_id) {
                    already_visited = 1;
                    break;
                }
            }

            if (already_visited || edge->probability < ctx->threshold) {
                edge_id = edge->next_edge;
                continue;
            }


            double prob = edge->probability * edge->weight;


            if (prob > best_prob) {
                best_prob = prob;
                best_next = edge->to_id;
                best_edge = edge_id;
            }

            edge_id = edge->next_edge;
        }


        if (best_next == 0 || best_prob < ctx->threshold) {
            break;
        }


        ArachnePrediction* pred = malloc(sizeof(ArachnePrediction));
        if (!pred) break;
        
        ArachneNode* next_node = arachne_get_node(graph, best_next);
        if (next_node) {
            pred->node_id = best_next;
            strncpy(pred->label, next_node->label, MAX_LABEL - 1);
            pred->label[MAX_LABEL - 1] = '\0';
            pred->probability = best_prob;
            pred->depth = depth;
            pred->edge_id = best_edge;
            pred->next = results;
            results = pred;

            if (visited_count < MAX_QUERY_DEPTH) {
                visited[visited_count++] = best_next;
            }

            if (graph->auto_learn && best_edge < graph->header->edge_count) {
                graph->edges[best_edge].occurrences++;
            }

            current = best_next;
            depth++;
        } else {
            free(pred);
            break;
        }
    }

    pthread_rwlock_unlock(&graph->lock);


    ArachnePrediction* reversed = NULL;
    ArachnePrediction* tmp = results;
    while (tmp) {
        ArachnePrediction* next = tmp->next;
        tmp->next = reversed;
        reversed = tmp;
        tmp = next;
    }

    if (reversed) {
        graph->header->total_predictions++;
    }

    return reversed;
}

// ============================================================
// PREDICT TOP K
// ============================================================
ArachnePrediction* arachne_predict_top_k(ArachneGraph* graph, ArachneContext* ctx) {
    return arachne_predict(graph, ctx);
}

// ============================================================
// FREE PREDICTION
// ============================================================
void arachne_free_prediction(ArachnePrediction* pred) {
    while (pred) {
        ArachnePrediction* next = pred->next;
        free(pred);
        pred = next;
    }
}

// ============================================================
// VALIDATE PREDICTION
// ============================================================
int arachne_validate_prediction(ArachneGraph* graph, ArachnePrediction* pred, bool success) {
    if (!graph || !pred) return -1;

    pthread_rwlock_wrlock(&graph->lock);

    if (pred->edge_id < graph->header->edge_count) {
        ArachneEdge* edge = &graph->edges[pred->edge_id];
        if (success) {
            edge->successes++;
        }
        if (edge->occurrences > 0) {
            double new_prob = (double)edge->successes / edge->occurrences;
            edge->probability = edge->probability * (1 - graph->learning_rate) +
                               new_prob * graph->learning_rate;
            if (edge->probability > 1.0) edge->probability = 1.0;
            if (edge->probability < 0.0) edge->probability = 0.0;
        }
        edge->updated_at = time(NULL);
        graph->is_dirty = true;
    }

    if (success) {
        graph->header->successful_predictions++;
    }

    pthread_rwlock_unlock(&graph->lock);
    return 0;
}
