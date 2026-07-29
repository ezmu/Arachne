#include "../include/arachne.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    time_t time;
    double value;
} TimePoint;

ArachnePrediction* arachne_predict_at_time(ArachneGraph* graph,
                                           ArachneContext* ctx,
                                           time_t target_time) {
    if (!graph || !ctx) return NULL;
    
    ArachneContext time_ctx = *ctx;
    time_ctx.use_time = true;
    time_ctx.time_from = target_time - 3600;
    time_ctx.time_to = target_time + 3600;
    
    return arachne_predict(graph, &time_ctx);
}

ArachnePrediction* arachne_trend_analysis(ArachneGraph* graph,
                                          uint64_t node_id,
                                          time_t from, time_t to) {
    if (!graph) return NULL;
    
    pthread_rwlock_rdlock(&graph->lock);
    
    ArachneNode* node = arachne_get_node(graph, node_id);
    if (!node) {
        pthread_rwlock_unlock(&graph->lock);
        return NULL;
    }
    
    TimePoint* points = malloc(100 * sizeof(TimePoint));
    int point_count = 0;
    
    uint64_t edge_id = node->first_edge;
    while (edge_id != 0 && point_count < 100) {
        ArachneEdge* edge = &graph->edges[edge_id];
        if (edge->created_at >= from && edge->created_at <= to) {
            points[point_count].time = edge->created_at;
            points[point_count].value = edge->probability;
            point_count++;
        }
        edge_id = edge->next_edge;
    }
    
    pthread_rwlock_unlock(&graph->lock);
    
    if (point_count == 0) {
        free(points);
        return NULL;
    }
    
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    double base_time = points[0].time;
    
    for (int i = 0; i < point_count; i++) {
        double x = points[i].time - base_time;
        double y = points[i].value;
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    double slope = 0;
    if (point_count * sum_x2 - sum_x * sum_x != 0) {
        slope = (point_count * sum_xy - sum_x * sum_y) / 
                (point_count * sum_x2 - sum_x * sum_x);
    }
    
    free(points);
    
    ArachnePrediction* result = malloc(sizeof(ArachnePrediction));
    result->node_id = node_id;
    strncpy(result->label, node->label, MAX_LABEL - 1);
    result->probability = 0.5 + slope * 10;
    if (result->probability < 0) result->probability = 0;
    if (result->probability > 1) result->probability = 1;
    result->depth = 0;
    result->edge_id = 0;
    result->next = NULL;
    
    return result;
}
