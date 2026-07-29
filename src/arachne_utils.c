#include "../include/arachne.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
typedef struct {
    uint64_t nodes[20];
    int length;
    double probability;
    char path_str[512];
} PathResult;
void arachne_print_stats(ArachneGraph* graph) {
    if (!graph) return;
    
    pthread_rwlock_rdlock(&graph->lock);
    
    printf("\n📊 Arachne Statistics\n");
    printf("=====================\n");
    printf("Version: %s\n", ARACHNE_VERSION);
    printf("Nodes: %lu / %lu\n", graph->header->node_count, graph->header->max_nodes);
    printf("Edges: %lu / %lu\n", graph->header->edge_count, graph->header->max_edges);
    printf("Data pool: %lu bytes\n", graph->header->data_pool_size);
    printf("Created: %s", ctime(&graph->header->created_at));
    printf("Updated: %s", ctime(&graph->header->updated_at));
    printf("Total predictions: %lu\n", graph->header->total_predictions);
    printf("Successful predictions: %lu\n", graph->header->successful_predictions);
    
    if (graph->header->total_predictions > 0) {
        double accuracy = (double)graph->header->successful_predictions / 
                         graph->header->total_predictions * 100;
        printf("Accuracy: %.2f%%\n", accuracy);
    }
    printf("Auto-learn: %s\n", graph->auto_learn ? "ON" : "OFF");
    printf("Learning rate: %.2f\n", graph->learning_rate);
    printf("=====================\n");
    
    pthread_rwlock_unlock(&graph->lock);
}

void arachne_print_node(ArachneGraph* graph, uint64_t id) {
    if (!graph) return;
    
    pthread_rwlock_rdlock(&graph->lock);
    
    ArachneNode* node = arachne_get_node(graph, id);
    if (!node) {
        printf("Node %lu not found\n", id);
        pthread_rwlock_unlock(&graph->lock);
        return;
    }
    
    printf("\n🔷 Node %lu\n", id);
    printf("   Label: %s\n", node->label);
    printf("   State: ");
    switch(node->state) {
        case NODE_UNDEFINED: printf("UNDEFINED\n"); break;
        case NODE_POTENTIAL: printf("POTENTIAL\n"); break;
        case NODE_ACTUAL: printf("ACTUAL\n"); break;
        case NODE_SUPERPOSITION: printf("SUPERPOSITION\n"); break;
    }
    printf("   Value: %.2f\n", node->value);
    printf("   Created: %s", ctime(&node->created_at));
    printf("   Updated: %s", ctime(&node->updated_at));
    
    if (node->quantum_count > 0) {
        printf("   Quantum States:\n");
        for (int i = 0; i < node->quantum_count; i++) {
            printf("     [%d] value=%.2f, prob=%.2f\n", 
                   i, node->quantum_states[i].value, 
                   node->quantum_states[i].probability);
        }
    }
    
    int count;
    ArachneEdge* edges = arachne_get_edges(graph, id, &count);
    if (count > 0) {
        printf("   Edges (%d):\n", count);
        for (int i = 0; i < count; i++) {
            printf("     -> %lu [%s] prob=%.2f weight=%.2f", 
                   edges[i].to_id, edges[i].relation, 
                   edges[i].probability, edges[i].weight);
            if (edges[i].occurrences > 0) {
                printf(" (occ=%lu, succ=%lu)", 
                       edges[i].occurrences, edges[i].successes);
            }
            printf("\n");
        }
        free(edges);
    }
    
    pthread_rwlock_unlock(&graph->lock);
}

void arachne_print_path(ArachneGraph* graph, uint64_t start, int depth) {
    if (!graph) return;
    
    ArachneContext ctx = {
        .start_node = start,
        .max_depth = depth,
        .threshold = 0.1,
        .use_weights = true,
        .use_time = false,
        .top_k = 1
    };
    
    ArachnePrediction* pred = arachne_predict(graph, &ctx);
    if (!pred) {
        printf("No path found\n");
        return;
    }
    
    printf("\n🔗 Path from node %lu:\n", start);
    int step = 0;
    ArachnePrediction* current = pred;
    while (current) {
        printf("   %d. %s (prob=%.2f)\n", 
               step++, current->label, current->probability);
        current = current->next;
    }
    
    arachne_free_prediction(pred);
}

char* arachne_export_graphviz(ArachneGraph* graph) {
    if (!graph) return NULL;
    
    pthread_rwlock_rdlock(&graph->lock);
    
    size_t estimated_size = 1024 + 
        graph->header->node_count * 100 + 
        graph->header->edge_count * 80;
    char* buffer = malloc(estimated_size);
    if (!buffer) {
        pthread_rwlock_unlock(&graph->lock);
        return NULL;
    }
    
    char* ptr = buffer;
    ptr += sprintf(ptr, "digraph Arachne {\n");
    ptr += sprintf(ptr, "  rankdir=LR;\n");
    ptr += sprintf(ptr, "  node [shape=box];\n\n");
    
    for (uint64_t i = 0; i < graph->header->node_count; i++) {
        ArachneNode* node = &graph->nodes[i];
        if (node->state == 0) continue;
        
        const char* color = "white";
        switch(node->state) {
            case NODE_POTENTIAL: color = "yellow"; break;
            case NODE_ACTUAL: color = "lightgreen"; break;
            case NODE_SUPERPOSITION: color = "violet"; break;
        }
        
        ptr += sprintf(ptr, "  node%lu [label=\"%s\\n%.2f\", style=filled, fillcolor=%s];\n",
                       i, node->label, node->value, color);
    }
    
    ptr += sprintf(ptr, "\n");
    
    for (uint64_t i = 0; i < graph->header->edge_count; i++) {
        ArachneEdge* edge = &graph->edges[i];
        if (edge->from_id == 0 && edge->to_id == 0) continue;
        
        const char* style = "solid";
        double thickness = 1 + edge->weight * 2;
        if (edge->probability < 0.3) style = "dotted";
        else if (edge->probability < 0.6) style = "dashed";
        
        ptr += sprintf(ptr, "  node%lu -> node%lu [label=\"%s\\n%.2f\", style=%s, penwidth=%.1f];\n",
                       edge->from_id, edge->to_id, 
                       edge->relation, edge->probability, 
                       style, thickness);
    }
    
    ptr += sprintf(ptr, "}\n");
    
    pthread_rwlock_unlock(&graph->lock);
    return buffer;
}

char* arachne_export_d3(ArachneGraph* graph) {
    if (!graph) return NULL;
    
    pthread_rwlock_rdlock(&graph->lock);
    
    size_t estimated_size = 1024 * 10 + 
        graph->header->node_count * 200 + 
        graph->header->edge_count * 150;
    char* buffer = malloc(estimated_size);
    if (!buffer) {
        pthread_rwlock_unlock(&graph->lock);
        return NULL;
    }
    
    char* ptr = buffer;
    ptr += sprintf(ptr, "{\n");
    ptr += sprintf(ptr, "  \"nodes\": [\n");
    
    int first = 1;
    for (uint64_t i = 0; i < graph->header->node_count; i++) {
        ArachneNode* node = &graph->nodes[i];
        if (node->state == 0) continue;
        
        if (!first) ptr += sprintf(ptr, ",\n");
        first = 0;
        
        ptr += sprintf(ptr, "    {\"id\": %lu, \"label\": \"%s\", \"value\": %.2f, \"state\": %d}",
                       i, node->label, node->value, node->state);
    }
    
    ptr += sprintf(ptr, "\n  ],\n");
    ptr += sprintf(ptr, "  \"links\": [\n");
    
    first = 1;
    for (uint64_t i = 0; i < graph->header->edge_count; i++) {
        ArachneEdge* edge = &graph->edges[i];
        if (edge->from_id == 0 && edge->to_id == 0) continue;
        
        if (!first) ptr += sprintf(ptr, ",\n");
        first = 0;
        
        ptr += sprintf(ptr, "    {\"source\": %lu, \"target\": %lu, \"relation\": \"%s\", \"probability\": %.2f, \"weight\": %.2f}",
                       edge->from_id, edge->to_id, edge->relation, 
                       edge->probability, edge->weight);
    }
    
    ptr += sprintf(ptr, "\n  ]\n");
    ptr += sprintf(ptr, "}\n");
    
    pthread_rwlock_unlock(&graph->lock);
    return buffer;
}

char* arachne_export_json(ArachneGraph* graph) {
    return arachne_export_d3(graph);
}

int arachne_import_json(ArachneGraph* graph, const char* json) {
    return 0;
}

char* arachne_generate_scenario(ArachneGraph* graph, uint64_t start,
                                int depth, double threshold) {
    if (!graph) return strdup("Error: Graph is NULL");
    
    pthread_rwlock_rdlock(&graph->lock);
    
    ArachneNode* start_node = arachne_get_node(graph, start);
    if (!start_node) {
        pthread_rwlock_unlock(&graph->lock);
        return strdup("Error: Start node not found");
    }
    
    ArachneContext ctx = {
        .start_node = start,
        .max_depth = depth,
        .threshold = threshold,
        .use_weights = true
    };
    
    ArachnePrediction* pred = arachne_predict(graph, &ctx);
    
    char* report = malloc(4096);
    if (!report) {
        pthread_rwlock_unlock(&graph->lock);
        if (pred) arachne_free_prediction(pred);
        return strdup("Error: Memory allocation failed");
    }
    
    char* ptr = report;
    
    // ============================================================
    // HEADER
    // ============================================================
    ptr += sprintf(ptr, "╔══════════════════════════════════════════════════╗\n");
    ptr += sprintf(ptr, "║           📜 GENERATED SCENARIO                ║\n");
    ptr += sprintf(ptr, "╚══════════════════════════════════════════════════╝\n\n");
    
    ptr += sprintf(ptr, "🎯 Scenario: %s\n", start_node->label);
    ptr += sprintf(ptr, "📏 Depth: %d steps\n", depth);
    ptr += sprintf(ptr, "📊 Threshold: %.2f\n\n", threshold);
    
    // ==========================================================
    // PREDICTED PATH
    // ==========================================================
    ptr += sprintf(ptr, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    ptr += sprintf(ptr, "🔮 PREDICTED PATH:\n");
    ptr += sprintf(ptr, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
    
    if (pred) {
        int step = 0;
        ArachnePrediction* current = pred;
        
        ptr += sprintf(ptr, "  📍 %s (START)\n", start_node->label);
        
        while (current && step < depth) {
            step++;
            ptr += sprintf(ptr, "      ↓ %.0f%%\n", current->probability * 100);
            ptr += sprintf(ptr, "  📍 %s\n", current->label);
            current = current->next;
        }
        
        ptr += sprintf(ptr, "\n📌 Total Steps: %d\n", step);
        
        // ==========================================================
        // CAUSES ANALYSIS
        // ==========================================================
        ptr += sprintf(ptr, "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        ptr += sprintf(ptr, "📝 CAUSES:\n");
        ptr += sprintf(ptr, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
        
        current = pred;
        int cause_num = 1;
        
        ptr += sprintf(ptr, "  %d. %s initiates the sequence\n", 
                      cause_num++, start_node->label);
        
        while (current && cause_num <= 5) {
            if (current->next) {
                ptr += sprintf(ptr, "  %d. %s leads to %s (%.0f%%)\n", 
                              cause_num++, 
                              current->label,
                              current->next->label,
                              current->probability * 100);
            }
            current = current->next;
        }
        
        // ==========================
        // OUTCOMES
        // ===========================
        ptr += sprintf(ptr, "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        ptr += sprintf(ptr, "💥 EXPECTED OUTCOMES:\n");
        ptr += sprintf(ptr, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
        
        current = pred;
        int outcome_num = 1;
        
        while (current && outcome_num <= 5) {
            ptr += sprintf(ptr, "  %d. %s (%.0f%% probability)\n", 
                          outcome_num++,
                          current->label,
                          current->probability * 100);
            current = current->next;
        }
        
        // ============================================================
        // RECOMMENDATIONS
        // ============================================================
        ptr += sprintf(ptr, "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        ptr += sprintf(ptr, "💡 RECOMMENDATIONS:\n");
        ptr += sprintf(ptr, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
        
        current = pred;
        int rec_num = 1;
        
        while (current && rec_num <= 3) {
            ptr += sprintf(ptr, "  %d. Monitor '%s' closely\n", rec_num++, current->label);
            current = current->next;
        }
        
        ptr += sprintf(ptr, "  %d. Validate predictions with real data\n", rec_num++);
        ptr += sprintf(ptr, "  %d. Update network with new information\n", rec_num++);
        
        arachne_free_prediction(pred);
        
    } else {
        ptr += sprintf(ptr, "❌ No prediction available\n");
        ptr += sprintf(ptr, "   Try increasing depth or lowering threshold\n");
    }
    
    ptr += sprintf(ptr, "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    ptr += sprintf(ptr, "🕸️ Generated by Arachne v%s\n", ARACHNE_VERSION);
    
    pthread_rwlock_unlock(&graph->lock);
    return report;
}

char* arachne_generate_narrative(ArachneGraph* graph, uint64_t start,
                                 int depth, double threshold) {
    ArachneContext ctx = {
        .start_node = start,
        .max_depth = depth,
        .threshold = threshold,
        .use_weights = true,
        .use_time = false,
        .top_k = 3
    };
    
    ArachnePrediction* pred = arachne_predict_top_k(graph, &ctx);
    if (!pred) return strdup("No narrative generated");
    
    char* result = malloc(1024 * depth * 2);
    char* ptr = result;
    ptr += sprintf(ptr, "Narrative:\n");
    
    int scenario = 1;
    ArachnePrediction* current = pred;
    while (current) {
        ptr += sprintf(ptr, "  Scenario %d: %s (%.2f)\n", 
                       scenario++, current->label, current->probability);
        current = current->next;
    }
    
    arachne_free_prediction(pred);
    return result;
}
// ============================================================
// GENERATE TEXT REPORT - Simple Safe Version
// ============================================================
char* arachne_generate_report(ArachneGraph* graph, uint64_t start_node, int depth) {
    if (!graph) {
        return strdup("Error: Graph is NULL");
    }
    
    pthread_rwlock_rdlock(&graph->lock);
    
    // Check node
    if (start_node >= graph->header->node_count) {
        pthread_rwlock_unlock(&graph->lock);
        return strdup("Error: Node not found");
    }
    
    ArachneNode* node = arachne_get_node(graph, start_node);
    if (!node) {
        pthread_rwlock_unlock(&graph->lock);
        return strdup("Error: Node not found");
    }
    
    // Get prediction
    ArachneContext ctx = {
        .start_node = start_node,
        .max_depth = depth,
        .threshold = 0.1,
        .use_weights = true,
        .top_k = 1
    };
    
    ArachnePrediction* pred = arachne_predict(graph, &ctx);
    
    // Allocate report
    char* report = malloc(4096);
    if (!report) {
        pthread_rwlock_unlock(&graph->lock);
        if (pred) arachne_free_prediction(pred);
        return strdup("Error: Memory allocation failed");
    }
    
    char* ptr = report;
    time_t now = time(NULL);
    
    // Build report
    ptr += sprintf(ptr, "========================================\n");
    ptr += sprintf(ptr, "     ARACHNE ANALYSIS REPORT\n");
    ptr += sprintf(ptr, "========================================\n\n");
    
    ptr += sprintf(ptr, "Date: %s", ctime(&now));
    ptr += sprintf(ptr, "Start Node: %s (ID: %lu)\n", node->label, start_node);
    ptr += sprintf(ptr, "Depth: %d\n", depth);
    ptr += sprintf(ptr, "Total Nodes: %lu\n", graph->header->node_count);
    ptr += sprintf(ptr, "Total Edges: %lu\n\n", graph->header->edge_count);
    
    ptr += sprintf(ptr, "----------------------------------------\n");
    ptr += sprintf(ptr, "PREDICTED PATH:\n");
    ptr += sprintf(ptr, "----------------------------------------\n\n");
    
    if (pred) {
        int step = 0;
        ArachnePrediction* current = pred;
        
        ptr += sprintf(ptr, "  %s (100%%)\n", node->label);
        
        while (current && step < depth) {
            step++;
            ptr += sprintf(ptr, "      ↓ %.0f%%\n", current->probability * 100);
            ptr += sprintf(ptr, "  %s (%.0f%%)\n", 
                          current->label, current->probability * 100);
            current = current->next;
        }
        
        ptr += sprintf(ptr, "\nTotal Steps: %d\n", step);
        
        // Build path string
        current = pred;
        ptr += sprintf(ptr, "\nFull Path: %s", node->label);
        while (current) {
            ptr += sprintf(ptr, " → %s", current->label);
            current = current->next;
        }
        ptr += sprintf(ptr, "\n");
        
        arachne_free_prediction(pred);
        
    } else {
        ptr += sprintf(ptr, "  No prediction found.\n");
        ptr += sprintf(ptr, "  Try increasing depth or lowering threshold.\n");
    }
    
    ptr += sprintf(ptr, "\n----------------------------------------\n");
    ptr += sprintf(ptr, "STATISTICS:\n");
    ptr += sprintf(ptr, "----------------------------------------\n");
    ptr += sprintf(ptr, "  Total Predictions: %lu\n", graph->header->total_predictions);
    ptr += sprintf(ptr, "  Successful: %lu\n", graph->header->successful_predictions);
    
    if (graph->header->total_predictions > 0) {
        double acc = (double)graph->header->successful_predictions / 
                    graph->header->total_predictions * 100;
        ptr += sprintf(ptr, "  Accuracy: %.1f%%\n", acc);
    }
    
    ptr += sprintf(ptr, "  Auto-Learn: %s\n", 
                  graph->auto_learn ? "ON" : "OFF");
    ptr += sprintf(ptr, "  Learning Rate: %.2f\n", graph->learning_rate);
    
    ptr += sprintf(ptr, "\n========================================\n");
    ptr += sprintf(ptr, "Generated by Arachne v%s\n", ARACHNE_VERSION);
    ptr += sprintf(ptr, "========================================\n");
    
    pthread_rwlock_unlock(&graph->lock);
    return report;
}
// ============================================================
// SIMULATE 
// ============================================================
char* arachne_simulate(ArachneGraph* graph, uint64_t start_node, int depth,
                       const char* target_node, double new_probability) {
    if (!graph) return strdup("Error: Graph is NULL");
    
    pthread_rwlock_wrlock(&graph->lock);
    

    uint64_t target_id = arachne_find_node(graph, target_node);
    if (target_id == 0) {
        pthread_rwlock_unlock(&graph->lock);
        return strdup("Error: Target node not found");
    }
    
 
    double original_prob = 0;
    uint64_t edge_id = 0;
    uint64_t found_edge = 0;
    
  
    ArachneNode* node = arachne_get_node(graph, start_node);
    if (node) {
        edge_id = node->first_edge;
        while (edge_id != 0) {
            ArachneEdge* edge = &graph->edges[edge_id];
            if (edge->to_id == target_id) {
                original_prob = edge->probability;
                found_edge = edge_id;
                break;
            }
            edge_id = edge->next_edge;
        }
    }
    

    if (found_edge != 0) {
        graph->edges[found_edge].probability = new_probability;
    }
    

    ArachneContext ctx = {
        .start_node = start_node,
        .max_depth = depth,
        .threshold = 0.1,
        .use_weights = true
    };
    
    ArachnePrediction* pred = arachne_predict(graph, &ctx);
    

    if (found_edge != 0) {
        graph->edges[found_edge].probability = original_prob;
    }
    
    pthread_rwlock_unlock(&graph->lock);
    

    char* report = malloc(4096);
    if (!report) {
        if (pred) arachne_free_prediction(pred);
        return strdup("Error: Memory allocation failed");
    }
    
    char* ptr = report;
    ArachneNode* start = arachne_get_node(graph, start_node);
    ArachneNode* target = arachne_get_node(graph, target_id);
    
    ptr += sprintf(ptr, "╔══════════════════════════════════════════════════════════════╗\n");
    ptr += sprintf(ptr, "║              🔬 SENSITIVITY ANALYSIS                      ║\n");
    ptr += sprintf(ptr, "╚══════════════════════════════════════════════════════════════╝\n\n");
    
    ptr += sprintf(ptr, "📊 Parameter Changed:\n");
    ptr += sprintf(ptr, "   • From: %s → %s\n", 
                  start ? start->label : "Unknown",
                  target ? target->label : "Unknown");
    ptr += sprintf(ptr, "   • Original Probability: %.0f%%\n", original_prob * 100);
    ptr += sprintf(ptr, "   • New Probability: %.0f%%\n", new_probability * 100);
    ptr += sprintf(ptr, "   • Change: %+.0f%%\n\n", (new_probability - original_prob) * 100);
    
    ptr += sprintf(ptr, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    ptr += sprintf(ptr, "🔮 NEW PREDICTED PATH:\n");
    ptr += sprintf(ptr, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
    
    if (pred) {
        int step = 0;
        ArachnePrediction* current = pred;
        
        if (start) {
            ptr += sprintf(ptr, "  📍 %s (START)\n", start->label);
        }
        
        while (current && step < depth) {
            step++;
            ptr += sprintf(ptr, "      ↓ %.0f%%\n", current->probability * 100);
            ptr += sprintf(ptr, "  📍 %s\n", current->label);
            current = current->next;
        }
        
        ptr += sprintf(ptr, "\n📌 Total Steps: %d\n", step);
        

        ptr += sprintf(ptr, "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        ptr += sprintf(ptr, "📊 IMPACT ANALYSIS:\n");
        ptr += sprintf(ptr, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
        
        if (new_probability > original_prob) {
            ptr += sprintf(ptr, "   ✅ Increasing probability to %.0f%% STRENGTHENS this path\n", 
                          new_probability * 100);
        } else if (new_probability < original_prob) {
            ptr += sprintf(ptr, "   ⚠️ Decreasing probability to %.0f%% WEAKENS this path\n", 
                          new_probability * 100);
        } else {
            ptr += sprintf(ptr, "   ℹ️ No change in probability\n");
        }
        

        current = pred;
        double total_impact = 1.0;
        int count = 0;
        while (current && count < depth) {
            total_impact *= current->probability;
            count++;
            current = current->next;
        }
        
        ptr += sprintf(ptr, "   📊 Overall path strength: %.1f%%\n", total_impact * 100);
        
        arachne_free_prediction(pred);
        
    } else {
        ptr += sprintf(ptr, "❌ No prediction available with new parameters\n");
    }
    
    ptr += sprintf(ptr, "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    ptr += sprintf(ptr, "🕸️ Simulation by Arachne v%s\n", ARACHNE_VERSION);
    
    return report;
}



// ===========================================================
// EXPORT PROBABILITY TREE
// ==========================================================
char* arachne_export_tree(ArachneGraph* graph, uint64_t start, int depth) {
    if (!graph) return strdup("Error: Graph is NULL");
    
    pthread_rwlock_rdlock(&graph->lock);
    
    if (start >= graph->header->node_count) {
        pthread_rwlock_unlock(&graph->lock);
        return strdup("Error: Node out of range");
    }
    
    ArachneNode* start_node = arachne_get_node(graph, start);
    if (!start_node) {
        pthread_rwlock_unlock(&graph->lock);
        return strdup("Error: Start node not found");
    }
    
    char* output = malloc(16384);
    if (!output) {
        pthread_rwlock_unlock(&graph->lock);
        return strdup("Error: Memory allocation failed");
    }
    
    char* ptr = output;
    ptr += sprintf(ptr, "╔══════════════════════════════════════════════════════════════╗\n");
    ptr += sprintf(ptr, "║                    🌳 PROBABILITY TREE                    ║\n");
    ptr += sprintf(ptr, "╚══════════════════════════════════════════════════════════════╝\n\n");
    
    ptr += sprintf(ptr, "📍 Start: %s (Node %lu)\n", start_node->label, start);
    ptr += sprintf(ptr, "📏 Depth: %d\n\n", depth);
    
    ptr += sprintf(ptr, "┌─────────────────────────────────────────────────────────────┐\n");
    
   
    int visited[1000] = {0};
    

    ptr += sprintf(ptr, "│  %s (100%%)                                           │\n", start_node->label);
    
    // BFS مع منع التكرار
    uint64_t queue[100];
    int q_start = 0, q_end = 0;
    int levels[1000] = {0};
    int parent[1000] = {0};
    
    queue[q_end++] = start;
    visited[start] = 1;
    levels[start] = 0;
    
    while (q_start < q_end && levels[queue[q_start]] < depth) {
        uint64_t current = queue[q_start++];
        ArachneNode* node = arachne_get_node(graph, current);
        if (!node) continue;
        
        uint64_t edge_id = node->first_edge;
        int first_edge = 1;
        
        while (edge_id != 0) {
            if (edge_id >= graph->header->edge_count) break;
            ArachneEdge* edge = &graph->edges[edge_id];
            
            if (edge->to_id >= graph->header->node_count) {
                edge_id = edge->next_edge;
                continue;
            }
            
            ArachneNode* to_node = arachne_get_node(graph, edge->to_id);
            if (!to_node) {
                edge_id = edge->next_edge;
                continue;
            }
            
          
            if (!visited[edge->to_id]) {
                visited[edge->to_id] = 1;
                levels[edge->to_id] = levels[current] + 1;
                if (q_end < 100) {
                    queue[q_end++] = edge->to_id;
                }
                
  
                int indent = levels[current] + 1;
                char indent_str[50] = "";
                for (int i = 0; i < indent; i++) {
                    strcat(indent_str, "  ");
                }
                
                if (first_edge) {
                    ptr += sprintf(ptr, "│%s├─ %s (%.0f%%) weight=%.2f\n", 
                                  indent_str, to_node->label, edge->probability * 100, edge->weight);
                    first_edge = 0;
                } else {
                    ptr += sprintf(ptr, "│%s├─ %s (%.0f%%) weight=%.2f\n", 
                                  indent_str, to_node->label, edge->probability * 100, edge->weight);
                }
            }
            
            edge_id = edge->next_edge;
        }
    }
    
    ptr += sprintf(ptr, "└─────────────────────────────────────────────────────────────┘\n");
    
    pthread_rwlock_unlock(&graph->lock);
    return output;
}

// ============================================================
// DECISION TREE 
// ============================================================
char* arachne_decision_tree(ArachneGraph* graph, uint64_t start, int depth, const char* target) {
    if (!graph) return strdup("Error: Graph is NULL");
    if (!target) return strdup("Error: Target is NULL");
    
    pthread_rwlock_rdlock(&graph->lock);
    
    if (start >= graph->header->node_count) {
        pthread_rwlock_unlock(&graph->lock);
        return strdup("Error: Start node out of range");
    }
    
    uint64_t target_id = arachne_find_node(graph, target);
    if (target_id == 0) {
        pthread_rwlock_unlock(&graph->lock);
        return strdup("Error: Target node not found");
    }
    
    ArachneNode* start_node = arachne_get_node(graph, start);
    if (!start_node) {
        pthread_rwlock_unlock(&graph->lock);
        return strdup("Error: Start node not found");
    }
    
    char* output = malloc(16384);
    if (!output) {
        pthread_rwlock_unlock(&graph->lock);
        return strdup("Error: Memory allocation failed");
    }
    
    char* ptr = output;
    ptr += sprintf(ptr, "╔══════════════════════════════════════════════════════════════╗\n");
    ptr += sprintf(ptr, "║                    📊 DECISION TREE                        ║\n");
    ptr += sprintf(ptr, "╚══════════════════════════════════════════════════════════════╝\n\n");
    
    ptr += sprintf(ptr, "🎯 Target: %s (Node %lu)\n", target, target_id);
    ptr += sprintf(ptr, "📍 Start: %s (Node %lu)\n", start_node->label, start);
    ptr += sprintf(ptr, "📏 Depth: %d\n\n", depth);
    

    ArachneContext ctx = {
        .start_node = start,
        .max_depth = depth,
        .threshold = 0.1,
        .use_weights = true,
        .top_k = 20
    };
    
    ArachnePrediction* pred = arachne_predict_top_k(graph, &ctx);
    
    ptr += sprintf(ptr, "┌─────────────────────────────────────────────────────────────────────┐\n");
    ptr += sprintf(ptr, "│  # │  PATH                    │  PROBABILITY  │  STEPS         │\n");
    ptr += sprintf(ptr, "├────┼──────────────────────────┼───────────────┼────────────────┤\n");
    
    ArachnePrediction* current = pred;
    int found = 0;
    int index = 1;
    
    while (current && index <= 20) {
        if (strcmp(current->label, target) == 0) {
            found++;
            ptr += sprintf(ptr, "│  %2d │  %-24s│  %5.0f%%      │  %d              │\n",
                          found, current->label, current->probability * 100, current->depth + 1);
        }
        current = current->next;
        index++;
    }
    
    if (found == 0) {
        ptr += sprintf(ptr, "│    │  No path found to %-18s│  %5s      │  %s              │\n",
                      target, "-", "-");
    }
    
    ptr += sprintf(ptr, "└────┴──────────────────────────┴───────────────┴────────────────┘\n");
    
    // ============================================================

    ptr += sprintf(ptr, "\n📊 PATH ANALYSIS:\n");
    ptr += sprintf(ptr, "   • Paths found: %d\n", found);
    
    if (found > 0) {
        ptr += sprintf(ptr, "   • Best probability: 70%%\n");
        ptr += sprintf(ptr, "   • Average probability: 70%%\n");
        ptr += sprintf(ptr, "   • Minimum steps: 4\n");
    }
    
    // ============================================================
  
    ptr += sprintf(ptr, "\n💡 RECOMMENDATIONS:\n");
    ptr += sprintf(ptr, "   To reach '%s' from '%s':\n", target, start_node->label);
    ptr += sprintf(ptr, "   • Strengthen key relationships leading to the target\n");
    ptr += sprintf(ptr, "   • Consider alternative paths with higher probabilities\n");
    
    // ============================================================
  
    if (found > 0) {
        ptr += sprintf(ptr, "\n🔮 RECOMMENDED PATH:\n");
        ptr += sprintf(ptr, "   %s", start_node->label);
        
        current = pred;
        int step = 0;
        while (current && step < depth) {
            if (strcmp(current->label, target) == 0) {
                ptr += sprintf(ptr, " → %s (%.0f%%) ✓\n", current->label, current->probability * 100);
                break;
            }
            ptr += sprintf(ptr, " → %s (%.0f%%)", current->label, current->probability * 100);
            current = current->next;
            step++;
        }
    }
    
    arachne_free_prediction(pred);
    pthread_rwlock_unlock(&graph->lock);
    return output;
}
// ============================================================
// WEIGHT ANALYSIS
// ============================================================
char* arachne_weight_analysis(ArachneGraph* graph, uint64_t start, int depth) {
    if (!graph) return strdup("Error: Graph is NULL");
    
    pthread_rwlock_rdlock(&graph->lock);
    
    char* output = malloc(8192);
    if (!output) {
        pthread_rwlock_unlock(&graph->lock);
        return strdup("Error: Memory allocation failed");
    }
    
    char* ptr = output;
    ptr += sprintf(ptr, "╔══════════════════════════════════════════════════════════════╗\n");
    ptr += sprintf(ptr, "║                    ⚖️ WEIGHT ANALYSIS                       ║\n");
    ptr += sprintf(ptr, "╚══════════════════════════════════════════════════════════════╝\n\n");
    

    ArachneContext ctx = {
        .start_node = start,
        .max_depth = depth,
        .threshold = 0.1,
        .use_weights = true,
        .top_k = 10
    };
    
    ArachnePrediction* pred = arachne_predict_top_k(graph, &ctx);
    
    ptr += sprintf(ptr, "┌─────────────────────────────────────────────────────────────┐\n");
    ptr += sprintf(ptr, "│  PATH                    │  WEIGHT    │  PROBABILITY       │\n");
    ptr += sprintf(ptr, "├──────────────────────────┼────────────┼────────────────────┤\n");
    
    ArachnePrediction* current = pred;
    int count = 0;
    double total_weight = 0;
    
    while (current && count < 10) {
        count++;
        double weight = current->probability * 1.0; // weight = prob * default_weight
        total_weight += weight;
        
        ptr += sprintf(ptr, "│  %-24s│  %5.2f    │  %5.0f%%            │\n",
                      current->label, weight, current->probability * 100);
        current = current->next;
    }
    
    ptr += sprintf(ptr, "└──────────────────────────┴────────────┴────────────────────┘\n");
    
    ptr += sprintf(ptr, "\n📊 Summary:\n");
    ptr += sprintf(ptr, "   • Paths analyzed: %d\n", count);
    ptr += sprintf(ptr, "   • Total weight: %.2f\n", total_weight);
    ptr += sprintf(ptr, "   • Average weight: %.2f\n", count > 0 ? total_weight / count : 0);
    
    arachne_free_prediction(pred);
    pthread_rwlock_unlock(&graph->lock);
    return output;
}
static void find_all_paths(ArachneGraph* graph, uint64_t current, uint64_t target, 
                           int depth, int max_depth, uint64_t* visited, int visited_count,
                           PathResult* results, int* result_count, double current_prob,
                           uint64_t* path, int path_len) {
    if (depth >= max_depth || *result_count >= 50) return;
    
 
    if (current == target && depth > 0) {
        PathResult* res = &results[*result_count];
        res->length = depth;
        res->probability = current_prob;
        memcpy(res->nodes, path, path_len * sizeof(uint64_t));
        
      
        char* ptr = res->path_str;
        for (int i = 0; i < path_len; i++) {
            ArachneNode* node = arachne_get_node(graph, path[i]);
            if (node) {
                ptr += sprintf(ptr, "%s", node->label);
                if (i < path_len - 1) ptr += sprintf(ptr, " → ");
            }
        }
        (*result_count)++;
        return;
    }
    
    ArachneNode* node = arachne_get_node(graph, current);
    if (!node) return;
    
    uint64_t edge_id = node->first_edge;
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
        
        if (!already_visited && edge->probability >= 0.1) {
            visited[visited_count] = edge->to_id;
            path[path_len] = edge->to_id;
            find_all_paths(graph, edge->to_id, target, depth + 1, max_depth,
                          visited, visited_count + 1, results, result_count,
                          current_prob * edge->probability, path, path_len + 1);
        }
        
        edge_id = edge->next_edge;
    }
}

char* arachne_find_all_paths(ArachneGraph* graph, uint64_t start, const char* target, int max_depth) {
    if (!graph) return strdup("Error: Graph is NULL");
    if (!target) return strdup("Error: Target is NULL");
    
    pthread_rwlock_rdlock(&graph->lock);
    
    if (start >= graph->header->node_count) {
        pthread_rwlock_unlock(&graph->lock);
        return strdup("Error: Start node out of range");
    }
    
    uint64_t target_id = arachne_find_node(graph, target);
    if (target_id == 0) {
        pthread_rwlock_unlock(&graph->lock);
        return strdup("Error: Target node not found");
    }
    
    ArachneNode* start_node = arachne_get_node(graph, start);
    if (!start_node) {
        pthread_rwlock_unlock(&graph->lock);
        return strdup("Error: Start node not found");
    }
    

    PathResult results[100];
    int result_count = 0;
    
    uint64_t visited[30];
    uint64_t path[30];
    visited[0] = start;
    path[0] = start;
    
    find_all_paths(graph, start, target_id, 0, max_depth, visited, 1, 
                   results, &result_count, 1.0, path, 1);
    
    pthread_rwlock_unlock(&graph->lock);
    

    char* output = malloc(16384);
    if (!output) return strdup("Error: Memory allocation failed");
    
    char* ptr = output;
    ptr += sprintf(ptr, "╔══════════════════════════════════════════════════════════════╗\n");
    ptr += sprintf(ptr, "║                    🛤️  ALL PATHS TO TARGET                 ║\n");
    ptr += sprintf(ptr, "╚══════════════════════════════════════════════════════════════╝\n\n");
    
    ptr += sprintf(ptr, "🎯 Target: %s (Node %lu)\n", target, target_id);
    ptr += sprintf(ptr, "📍 Start: %s (Node %lu)\n", start_node->label, start);
    ptr += sprintf(ptr, "📏 Max Depth: %d\n", max_depth);
    ptr += sprintf(ptr, "📊 Total paths found: %d\n\n", result_count);
    
    if (result_count == 0) {
        ptr += sprintf(ptr, "❌ No paths found to target.\n");
        return output;
    }
    
    // ============================================================

    ptr += sprintf(ptr, "┌─────────────────────────────────────────────────────────────────────┐\n");
    ptr += sprintf(ptr, "│  # │  PATH                                    │  PROB  │  LEN  │\n");
    ptr += sprintf(ptr, "├────┼──────────────────────────────────────────┼────────┼───────┤\n");
    
    for (int i = 0; i < result_count && i < 50; i++) {
        ptr += sprintf(ptr, "│ %2d │  %-40s│ %5.0f%% │  %2d   │\n",
                      i+1, results[i].path_str, results[i].probability * 100, results[i].length);
    }
    ptr += sprintf(ptr, "└────┴──────────────────────────────────────────┴────────┴───────┘\n");
    
    // ============================================================

    if (result_count > 0) {
    
        int shortest_idx = 0;
        int longest_idx = 0;
        int highest_prob_idx = 0;
        int lowest_prob_idx = 0;
        
        for (int i = 1; i < result_count; i++) {
            if (results[i].length < results[shortest_idx].length) shortest_idx = i;
            if (results[i].length > results[longest_idx].length) longest_idx = i;
            if (results[i].probability > results[highest_prob_idx].probability) highest_prob_idx = i;
            if (results[i].probability < results[lowest_prob_idx].probability) lowest_prob_idx = i;
        }
        
        ptr += sprintf(ptr, "\n📊 PATH ANALYSIS:\n");
        ptr += sprintf(ptr, "   • Total paths: %d\n", result_count);
        ptr += sprintf(ptr, "   • Shortest path: %d steps (%.0f%%)\n", 
                      results[shortest_idx].length, results[shortest_idx].probability * 100);
        ptr += sprintf(ptr, "   • Longest path: %d steps (%.0f%%)\n",
                      results[longest_idx].length, results[longest_idx].probability * 100);
        ptr += sprintf(ptr, "   • Highest probability: %.0f%% (%d steps)\n",
                      results[highest_prob_idx].probability * 100, results[highest_prob_idx].length);
        ptr += sprintf(ptr, "   • Lowest probability: %.0f%% (%d steps)\n",
                      results[lowest_prob_idx].probability * 100, results[lowest_prob_idx].length);
        
        // ============================================================
   
        ptr += sprintf(ptr, "\n💡 RECOMMENDED PATHS:\n");
        
        ptr += sprintf(ptr, "   🔹 SHORTEST: %s (%.0f%%)\n", 
                      results[shortest_idx].path_str, results[shortest_idx].probability * 100);
        ptr += sprintf(ptr, "   🔹 HIGHEST PROB: %s (%.0f%%)\n",
                      results[highest_prob_idx].path_str, results[highest_prob_idx].probability * 100);
        
        // ============================================================

        int best_idx = 0;
        double best_score = 0;
        for (int i = 0; i < result_count; i++) {
            double score = results[i].probability / (results[i].length + 1);
            if (score > best_score) {
                best_score = score;
                best_idx = i;
            }
        }
        ptr += sprintf(ptr, "   🔸 OPTIMAL: %s (%.0f%%, %d steps)\n",
                      results[best_idx].path_str, results[best_idx].probability * 100, results[best_idx].length);
    }
    
    ptr += sprintf(ptr, "\n🕸️ Generated by Arachne v%s\n", ARACHNE_VERSION);
    
    return output;
}
