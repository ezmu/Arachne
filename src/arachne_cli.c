#include "../include/arachne.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_help() {
    printf("\n🕸️ Arachne CLI Commands:\n");
    printf("==========================\n");
    printf("  help                  - Show this help\n");
    printf("  stats                 - Show graph statistics\n");
    printf("  add <label> <value>   - Add a node\n");
    printf("  link <from> <to> <rel> <prob> - Add an edge\n");
    printf("  predict <id> <depth>  - Predict from node\n");
    printf("  show <id>             - Show node details\n");
    printf("  path <id> <depth>     - Show path from node\n");
    printf("  quantum <id> <value> <prob> - Add quantum state\n");
    printf("  collapse <id>         - Collapse quantum state\n");
    printf("  search <pattern>      - Search nodes\n");
    printf("  export <format>       - Export (json, graphviz, d3)\n");
    printf("  learn <id> <event> <impact> - Learn from event\n");
    printf("  accuracy              - Show prediction accuracy\n");
    printf("  anomalies             - Detect anomalies\n");
    printf("  report <id> <depth>   - Generate text report\n");
    printf("  generate <id> <depth> [threshold] - Generate scenario\n");
  printf("  simulate <id> <depth> <target> <prob> - Change probability and analyze\n");
printf("  batch <id> <depth> <target> <start> [end] [step] - Batch simulation\n");
printf("  tree <id> <depth>     - Show probability tree\n");
printf("  decision <id> <depth> <target> - Decision tree to target\n");
printf("  paths <id> <target> <depth> - Find all paths to target\n");

printf("  weight <id> <depth>   - Weight analysis\n");
    printf("  quit                  - Exit\n");
    printf("==========================\n");
}

static void process_command(ArachneGraph* graph, char* cmd) {
    char* args[10];
    int argc = 0;
    char* token = strtok(cmd, " ");
    
    while (token && argc < 10) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }
    
    if (argc == 0) return;
    
    if (strcmp(args[0], "help") == 0) {
        print_help();
    }
else if (strcmp(args[0], "unify") == 0) {

    uint64_t root = arachne_add_node(graph, "UNIVERSE", NODE_ACTUAL, 1.0);
    

    int count = 0;
    for (uint64_t i = 0; i < graph->header->node_count - 1; i++) {
        if (arachne_add_edge(graph, root, i, "contains", 1.0) == 0) {
            count++;
        }
    }
    
    printf("✅ Universe created with %d connections\n", count);
}
else if (strcmp(args[0], "generate") == 0 && argc >= 3) {
    uint64_t id = atoi(args[1]);
    int depth = atoi(args[2]);
    double threshold = (argc >= 4) ? atof(args[3]) : 0.1;
    char* scenario = arachne_generate_scenario(graph, id, depth, threshold);
    if (scenario) {
        printf("%s\n", scenario);
        free(scenario);
    }
}
else if (strcmp(args[0], "simulate") == 0 && argc >= 5) {
    uint64_t id = atoi(args[1]);
    int depth = atoi(args[2]);
    const char* target = args[3];
    double new_prob = atof(args[4]);
    char* result = arachne_simulate(graph, id, depth, target, new_prob);
    if (result) {
        printf("%s\n", result);
        free(result);
    }
}
else if (strcmp(args[0], "batch") == 0 && argc >= 5) {
    uint64_t id = atoi(args[1]);
    int depth = atoi(args[2]);
    const char* target = args[3];
    double start_prob = atof(args[4]);
    double end_prob = (argc >= 6) ? atof(args[5]) : 1.0;
    double step = (argc >= 7) ? atof(args[6]) : 0.1;
    
    ArachneNode* start_node = arachne_get_node(graph, id);
    char start_label[128] = "Unknown";
    if (start_node) {
        strncpy(start_label, start_node->label, 127);
        start_label[127] = '\0';
    }
    
    uint64_t target_id = arachne_find_node(graph, target);
    if (target_id == 0) {
        printf("❌ Target node '%s' not found\n", target);
        return;
    }
    
    printf("\n╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                    🔬 BATCH SIMULATION ANALYSIS                            ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n\n");
    
    // ============================================================

    // ===========================================
    printf("📊 Understanding Probabilities (%%):\n");
    printf("   •  0-30%%:  Very Weak (unlikely)\n");
    printf("   • 30-50%%:  Weak (low chance)\n");
    printf("   • 50-70%%:  Moderate (possible)\n");
    printf("   • 70-90%%:  Strong (highly likely)\n");
    printf("   • 90-100%%: Very Strong (almost certain)\n\n");
    
    printf("📊 Parameter: %s → %s\n", start_label, target);
    printf("📈 Range: %.0f%% → %.0f%% (step: %.0f%%)\n\n", 
           start_prob * 100, end_prob * 100, step * 100);
    
   
    typedef struct {
        double prob;
        char first_step[64];
        char full_path[256];
        int steps;
        double overall_prob;
        char strength[20];
    } Result;
    
    Result results[50];
    int result_count = 0;
    
 
    uint64_t edge_ids[50];
    double orig_probs[50];
    int edge_count = 0;
    
    ArachneNode* node = arachne_get_node(graph, id);
    if (node) {
        uint64_t eid = node->first_edge;
        while (eid != 0 && edge_count < 50) {
            edge_ids[edge_count] = eid;
            orig_probs[edge_count] = graph->edges[eid].probability;
            edge_count++;
            eid = graph->edges[eid].next_edge;
        }
    }
    
    printf("┌───────────┬───────────┬────────────────────────────────────────────────────┐\n");
    printf("│  Prob %s  │  Steps    │  Path                                              │\n", target);
    printf("├───────────┼───────────┼────────────────────────────────────────────────────┤\n");
    
    for (double p = start_prob; p <= end_prob + 0.001; p += step) {
        double prob = (p > 1.0) ? 1.0 : p;
        
    
        for (int i = 0; i < edge_count; i++) {
            if (graph->edges[edge_ids[i]].to_id == target_id) {
                graph->edges[edge_ids[i]].probability = prob;
            }
        }
        
   
        ArachneContext ctx = {
            .start_node = id,
            .max_depth = depth,
            .threshold = 0.1,
            .use_weights = true
        };
        
        ArachnePrediction* pred = arachne_predict(graph, &ctx);
        
        char first_step[64] = "None";
        char full_path[256] = {0};
        char* ptr = full_path;
        int steps = 0;
        double overall = 1.0;
        char strength[20] = "Unknown";
        
        if (pred) {
            strncpy(first_step, pred->label, 63);
            first_step[63] = '\0';
            
            ArachnePrediction* current = pred;
            ptr += sprintf(ptr, "%s", start_label);
            while (current && steps < depth) {
                steps++;
                overall *= current->probability;
                ptr += sprintf(ptr, " → %s", current->label);
                current = current->next;
            }
            
            arachne_free_prediction(pred);
        }
        

        if (prob < 0.30) strcpy(strength, "Very Weak");
        else if (prob < 0.50) strcpy(strength, "Weak");
        else if (prob < 0.70) strcpy(strength, "Moderate");
        else if (prob < 0.90) strcpy(strength, "Strong");
        else strcpy(strength, "Very Strong");
        
        results[result_count].prob = prob;
        strcpy(results[result_count].first_step, first_step);
        strcpy(results[result_count].full_path, full_path);
        results[result_count].steps = steps;
        results[result_count].overall_prob = overall;
        strcpy(results[result_count].strength, strength);
        result_count++;
        
        printf("│  %5.0f%%   │    %d    │  %-50s│\n", 
               prob * 100, steps, full_path);
        fflush(stdout);
    }
    

    for (int i = 0; i < edge_count; i++) {
        graph->edges[edge_ids[i]].probability = orig_probs[i];
    }
    
    printf("└───────────┴───────────┴────────────────────────────────────────────────────┘\n");
    
    // ============================================================
 
    printf("\n╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                    📊 CHANGE DETECTION                                     ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n\n");
    
    int changes = 0;
    char last_path[256] = {0};
    
    for (int i = 0; i < result_count; i++) {
        if (i == 0) {
            printf("   🔹 At %5.0f%% (%s): %s\n", 
                   results[i].prob * 100, results[i].strength, results[i].full_path);
            strcpy(last_path, results[i].full_path);
        } else if (strcmp(results[i].full_path, last_path) != 0) {
            printf("   🔸 At %5.0f%% (%s): %s  ← PATH CHANGED!\n", 
                   results[i].prob * 100, results[i].strength, results[i].full_path);
            strcpy(last_path, results[i].full_path);
            changes++;
        } else {
           
        }
    }
    
    if (changes == 0) {
        printf("   ℹ️ No path changes detected.\n");
        printf("   💡 Path is stable: %s\n", results[0].full_path);
    } else {
        printf("\n   📌 Total path changes: %d\n", changes);
    }
    
    // ==========================================================

    printf("\n╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                    🎯 CRITICAL THRESHOLD                                   ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n\n");
    
    int threshold_found = 0;
    for (int i = 1; i < result_count; i++) {
        if (strcmp(results[i].full_path, results[0].full_path) != 0) {
            double mid = (results[i-1].prob + results[i].prob) / 2;
            printf("   🎯 Critical threshold: ~%.0f%%\n", mid * 100);
            printf("      Below %.0f%% (%s): %s\n", 
                   results[i-1].prob * 100, results[i-1].strength, results[i-1].full_path);
            printf("      Above %.0f%% (%s): %s\n", 
                   results[i].prob * 100, results[i].strength, results[i].full_path);
            threshold_found = 1;
            break;
        }
    }
    
    if (!threshold_found) {
        printf("   ℹ️ No critical threshold found.\n");
        printf("   💡 Path stable across all probabilities.\n");
    }
    
    // ============================================================

    printf("\n╔══════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                    📋 SUMMARY                                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════════════════════╝\n\n");
    
    printf("   • Parameter: %s → %s\n", start_label, target);
    printf("   • Probability range: %.0f%% → %.0f%%\n", start_prob * 100, end_prob * 100);
    printf("   • Simulations: %d\n", result_count);
    printf("   • Path changes: %d\n", changes);
    printf("   • Most stable path: %s\n", results[0].full_path);
    
    if (changes > 0) {
        printf("   • Critical threshold: ~%.0f%%\n", 
               (results[0].prob + results[1].prob) / 2 * 100);
    }
    
    printf("\n💡 Remember: Probabilities represent strength of relationship.\n");
    printf("   • 0-30%%: Very Weak | 30-50%%: Weak | 50-70%%: Moderate\n");
    printf("   • 70-90%%: Strong | 90-100%%: Very Strong\n");
    
    printf("\n✅ Batch simulation complete!\n");
}
    else if (strcmp(args[0], "stats") == 0) {
        arachne_print_stats(graph);
    }
else if (strcmp(args[0], "debug") == 0) {
    arachne_debug_edges(graph);
}
else if (strcmp(args[0], "tree") == 0 && argc >= 3) {
    uint64_t id = atoi(args[1]);
    int depth = atoi(args[2]);
    char* output = arachne_export_tree(graph, id, depth);
    if (output) {
        printf("%s\n", output);
        free(output);
    }
}

else if (strcmp(args[0], "paths") == 0 && argc >= 4) {
    uint64_t id = atoi(args[1]);
    const char* target = args[2];
    int depth = atoi(args[3]);
    char* output = arachne_find_all_paths(graph, id, target, depth);
    if (output) {
        printf("%s\n", output);
        free(output);
    }
}
else if (strcmp(args[0], "decision") == 0 && argc >= 4) {
    uint64_t id = atoi(args[1]);
    int depth = atoi(args[2]);
    const char* target = args[3];
    char* output = arachne_decision_tree(graph, id, depth, target);
    if (output) {
        printf("%s\n", output);
        free(output);
    }
}
else if (strcmp(args[0], "weight") == 0 && argc >= 3) {
    uint64_t id = atoi(args[1]);
    int depth = atoi(args[2]);
    char* output = arachne_weight_analysis(graph, id, depth);
    if (output) {
        printf("%s\n", output);
        free(output);
    }
}
else if (strcmp(args[0], "report") == 0 && argc >= 3) {
    uint64_t id = atoi(args[1]);
    int depth = atoi(args[2]);
    char* report = arachne_generate_report(graph, id, depth);
    if (report) {
        printf("%s\n", report);
        free(report);
    }
}
    else if (strcmp(args[0], "add") == 0 && argc >= 3) {
        double value = atof(args[2]);
        uint64_t id = arachne_add_node(graph, args[1], NODE_ACTUAL, value);
        printf("✅ Node added: %lu\n", id);
    }
    else if (strcmp(args[0], "link") == 0 && argc >= 5) {
        uint64_t from = atoi(args[1]);
        uint64_t to = atoi(args[2]);
        double prob = atof(args[4]);
        if (arachne_add_edge(graph, from, to, args[3], prob) == 0) {
            printf("✅ Edge added\n");
        } else {
            printf("❌ Failed to add edge\n");
        }
    }
    else if (strcmp(args[0], "predict") == 0 && argc >= 3) {
        uint64_t id = atoi(args[1]);
        int depth = atoi(args[2]);
        ArachneContext ctx = {
            .start_node = id,
            .max_depth = depth,
            .threshold = 0.1
        };
        ArachnePrediction* pred = arachne_predict(graph, &ctx);
        if (pred) {
            printf("🔮 Prediction:\n");
            int step = 0;
            while (pred) {
                printf("  %d. %s (%.2f)\n", step++, pred->label, pred->probability);
                pred = pred->next;
            }
            arachne_free_prediction(pred);
        } else {
            printf("No prediction found\n");
        }
    }
    else if (strcmp(args[0], "show") == 0 && argc >= 2) {
        uint64_t id = atoi(args[1]);
        arachne_print_node(graph, id);
    }
    else if (strcmp(args[0], "path") == 0 && argc >= 3) {
        uint64_t id = atoi(args[1]);
        int depth = atoi(args[2]);
        arachne_print_path(graph, id, depth);
    }
    else if (strcmp(args[0], "quantum") == 0 && argc >= 4) {
        uint64_t id = atoi(args[1]);
        double value = atof(args[2]);
        double prob = atof(args[3]);
        if (arachne_add_quantum_state(graph, id, value, prob) == 0) {
            printf("✅ Quantum state added\n");
        } else {
            printf("❌ Failed to add quantum state\n");
        }
    }
    else if (strcmp(args[0], "collapse") == 0 && argc >= 2) {
        uint64_t id = atoi(args[1]);
        if (arachne_collapse_node(graph, id) == 0) {
            printf("✅ Quantum state collapsed\n");
        } else {
            printf("❌ Failed to collapse\n");
        }
    }
    else if (strcmp(args[0], "search") == 0 && argc >= 2) {
        int count;
        uint64_t* results = arachne_search_nodes(graph, args[1], &count);
        printf("🔍 Found %d nodes:\n", count);
        for (int i = 0; i < count; i++) {
            ArachneNode* node = arachne_get_node(graph, results[i]);
            if (node) {
                printf("  %lu: %s\n", results[i], node->label);
            }
        }
        free(results);
    }
    else if (strcmp(args[0], "export") == 0 && argc >= 2) {
        char* output = NULL;
        if (strcmp(args[1], "json") == 0) {
            output = arachne_export_json(graph);
        } else if (strcmp(args[1], "graphviz") == 0) {
            output = arachne_export_graphviz(graph);
        } else if (strcmp(args[1], "d3") == 0) {
            output = arachne_export_d3(graph);
        }
        if (output) {
            printf("%s\n", output);
            free(output);
        }
    }
    else if (strcmp(args[0], "learn") == 0 && argc >= 4) {
        uint64_t id = atoi(args[1]);
        double impact = atof(args[3]);
        if (arachne_learn(graph, id, args[2], impact) == 0) {
            printf("✅ Learning applied\n");
        } else {
            printf("❌ Failed to learn\n");
        }
    }
    else if (strcmp(args[0], "accuracy") == 0) {
        double acc = arachne_calculate_accuracy(graph);
        printf("📊 Prediction accuracy: %.2f%%\n", acc * 100);
    }
    else if (strcmp(args[0], "anomalies") == 0) {
        uint64_t* anomalies;
        int count;
        if (arachne_detect_anomalies(graph, &anomalies, &count) == 0) {
            printf("🔍 Found %d anomalies:\n", count);
            for (int i = 0; i < count; i++) {
                printf("  Node %lu\n", anomalies[i]);
            }
            free(anomalies);
        }
    }
    else if (strcmp(args[0], "quit") == 0 || strcmp(args[0], "exit") == 0) {
        printf("Goodbye!\n");
        exit(0);
    }
    else {
        printf("Unknown command. Type 'help' for commands.\n");
    }
}

int main(int argc, char** argv) {
    printf("🕸️ Arachne CLI v%s\n", ARACHNE_VERSION);
    printf("Type 'help' for commands\n\n");
    
    const char* filename = (argc > 1) ? argv[1] : "arachne.db";
    ArachneGraph* graph = arachne_open(filename);
    
    if (!graph) {
        fprintf(stderr, "Failed to open graph\n");
        return 1;
    }
    
    printf("✅ Graph opened: %s\n", filename);
    arachne_print_stats(graph);
    
    char input[1024];
    while (1) {
        printf("arachne> ");
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0;
        process_command(graph, input);
    }
    
    arachne_close(graph);
    return 0;
}
