#include "../include/arachne.h"
#include <stdlib.h>
#include <string.h>

ArachneQueryResult* arachne_query(ArachneGraph* graph, ArachneQuery* query) {
    if (!graph || !query) return NULL;
    
    pthread_rwlock_rdlock(&graph->lock);
    
    ArachneQueryResult* results = malloc(sizeof(ArachneQueryResult));
    results->node_ids = malloc(graph->header->node_count * sizeof(uint64_t));
    results->node_count = 0;
    results->paths = NULL;
    results->path_count = 0;
    results->next = NULL;
    
    int limit = query->limit > 0 ? query->limit : 100;
    
    for (uint64_t i = 0; i < graph->header->node_count && results->node_count < limit; i++) {
        ArachneNode* node = &graph->nodes[i];
        if (node->state == 0) continue;
        
        if (query->pattern && query->pattern[0] != '\0') {
            if (!strstr(node->label, query->pattern)) {
                continue;
            }
        }
        
        results->node_ids[results->node_count++] = i;
    }
    
    if (query->return_paths && results->node_count > 0) {
        results->paths = malloc(results->node_count * sizeof(char*));
        results->path_count = results->node_count;
        
        for (int i = 0; i < results->node_count; i++) {
            char* path = malloc(1024);
            ArachneNode* node = arachne_get_node(graph, results->node_ids[i]);
            if (node) {
                sprintf(path, "Node %lu: %s (value=%.2f)", 
                        results->node_ids[i], node->label, node->value);
            } else {
                sprintf(path, "Node %lu", results->node_ids[i]);
            }
            results->paths[i] = path;
        }
    }
    
    pthread_rwlock_unlock(&graph->lock);
    return results;
}

void arachne_free_query_result(ArachneQueryResult* result) {
    if (!result) return;
    
    if (result->node_ids) free(result->node_ids);
    
    if (result->paths) {
        for (int i = 0; i < result->path_count; i++) {
            if (result->paths[i]) free(result->paths[i]);
        }
        free(result->paths);
    }
    
    free(result);
}
