#include "../include/arachne.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <math.h>

// ============================================================
// INTERNAL FUNCTIONS
// ============================================================
static uint32_t calculate_checksum(void* data, size_t len) {
    uint32_t sum = 0;
    unsigned char* bytes = (unsigned char*)data;
    for (size_t i = 0; i < len; i++) {
        sum += bytes[i];
    }
    return sum;
}

static void expand_file(ArachneGraph* graph, size_t additional) {
    if (!graph) return;
    
    size_t new_size = graph->file_size + additional;
    if (ftruncate(graph->fd, new_size) != 0) {
        perror("ftruncate");
        return;
    }
    
    if (graph->mapped_data) {
        munmap(graph->mapped_data, graph->file_size);
    }
    
    graph->file_size = new_size;
    graph->mapped_data = mmap(NULL, graph->file_size,
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED, graph->fd, 0);
    if (graph->mapped_data == MAP_FAILED) {
        perror("mmap");
        graph->mapped_data = NULL;
        return;
    }
    
    graph->header = (ArachneHeader*)graph->mapped_data;
    graph->nodes = (ArachneNode*)((char*)graph->mapped_data + sizeof(ArachneHeader));
    graph->edges = (ArachneEdge*)((char*)graph->mapped_data + sizeof(ArachneHeader) +
                                  MAX_NODES * sizeof(ArachneNode));
    graph->data_pool = (char*)(graph->mapped_data + graph->header->data_pool_offset);
}

// ============================================================
// OPEN / CREATE
// ============================================================
ArachneGraph* arachne_open(const char* filename) {
    if (!filename) {
        fprintf(stderr, "Error: filename is NULL\n");
        return NULL;
    }
    
    ArachneGraph* graph = calloc(1, sizeof(ArachneGraph));
    if (!graph) {
        perror("calloc");
        return NULL;
    }

    pthread_rwlock_init(&graph->lock, NULL);
    graph->auto_learn = true;
    graph->learning_rate = 0.1;

    graph->fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (graph->fd < 0) {
        perror("open");
        free(graph);
        return NULL;
    }

    struct stat st;
    if (fstat(graph->fd, &st) != 0) {
        perror("fstat");
        close(graph->fd);
        free(graph);
        return NULL;
    }
    graph->file_size = st.st_size;

    if (graph->file_size == 0) {
        graph->file_size = 1024 * 1024 * 10;
        if (ftruncate(graph->fd, graph->file_size) != 0) {
            perror("ftruncate");
            close(graph->fd);
            free(graph);
            return NULL;
        }

        ArachneHeader header = {
            .magic = ARACHNE_MAGIC,
            .version = 1,
            .node_count = 0,
            .edge_count = 0,
            .max_nodes = MAX_NODES,
            .max_edges = MAX_EDGES,
            .data_pool_offset = sizeof(ArachneHeader) +
                                MAX_NODES * sizeof(ArachneNode) +
                                MAX_EDGES * sizeof(ArachneEdge),
            .data_pool_size = graph->file_size - sizeof(ArachneHeader) -
                              MAX_NODES * sizeof(ArachneNode) -
                              MAX_EDGES * sizeof(ArachneEdge),
            .free_node = 0,
            .free_edge = 0,
            .created_at = time(NULL),
            .updated_at = time(NULL),
            .checksum = 0,
            .total_predictions = 0,
            .successful_predictions = 0
        };

        if (pwrite(graph->fd, &header, sizeof(header), 0) != sizeof(header)) {
            perror("pwrite header");
            close(graph->fd);
            free(graph);
            return NULL;
        }

        ArachneNode empty_node = {0};
        for (int i = 0; i < MAX_NODES; i++) {
            off_t offset = sizeof(header) + i * sizeof(ArachneNode);
            if (pwrite(graph->fd, &empty_node, sizeof(ArachneNode), offset) != sizeof(ArachneNode)) {
                perror("pwrite node");
                close(graph->fd);
                free(graph);
                return NULL;
            }
        }

        ArachneEdge empty_edge = {0};
        for (int i = 0; i < MAX_EDGES; i++) {
            off_t offset = sizeof(header) + MAX_NODES * sizeof(ArachneNode) + i * sizeof(ArachneEdge);
            if (pwrite(graph->fd, &empty_edge, sizeof(ArachneEdge), offset) != sizeof(ArachneEdge)) {
                perror("pwrite edge");
                close(graph->fd);
                free(graph);
                return NULL;
            }
        }
    }


    // ============================================================
    graph->mapped_data = mmap(NULL, graph->file_size,
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED, graph->fd, 0);
    if (graph->mapped_data == MAP_FAILED) {
        perror("mmap");
        close(graph->fd);
        free(graph);
        return NULL;
    }


    graph->header = (ArachneHeader*)graph->mapped_data;
    

    size_t header_size = sizeof(ArachneHeader);
    size_t node_offset = header_size;
    size_t edge_offset = node_offset + (MAX_NODES * sizeof(ArachneNode));
    size_t data_offset = edge_offset + (MAX_EDGES * sizeof(ArachneEdge));
    
    graph->nodes = (ArachneNode*)((char*)graph->mapped_data + node_offset);
    graph->edges = (ArachneEdge*)((char*)graph->mapped_data + edge_offset);
    graph->data_pool = (char*)(graph->mapped_data + data_offset);
    

    if (graph->header->data_pool_offset == 0) {
        graph->header->data_pool_offset = data_offset;
        graph->header->data_pool_size = graph->file_size - data_offset;
    }



    // ============================================================
    printf("📊 Debug: Node count = %lu, Edge count = %lu\n", 
           graph->header->node_count, graph->header->edge_count);
    printf("📊 Debug: Node offset = %zu, Edge offset = %zu\n", 
           node_offset, edge_offset);
    

    if (graph->header->node_count > 0) {
        printf("📊 Debug: Node 0 first_edge = %lu\n", graph->nodes[0].first_edge);
        if (graph->nodes[0].first_edge > 0) {
            ArachneEdge* edge = &graph->edges[graph->nodes[0].first_edge];
            printf("📊 Debug: Edge %lu: %lu -> %lu [%s]\n", 
                   graph->nodes[0].first_edge, edge->from_id, edge->to_id, edge->relation);
        }
    }

    return graph;
}
// ============================================================
// CLOSE
// ============================================================
void arachne_close(ArachneGraph* graph) {
    if (!graph) return;

    if (graph->is_dirty) {
        arachne_sync(graph);
    }

    if (graph->mapped_data) {
        munmap(graph->mapped_data, graph->file_size);
    }
    
    if (graph->fd >= 0) {
        close(graph->fd);
    }
    
    pthread_rwlock_destroy(&graph->lock);
    free(graph);
}

// ============================================================
// SYNC
// ============================================================
int arachne_sync(ArachneGraph* graph) {
    if (!graph) return -1;

    pthread_rwlock_wrlock(&graph->lock);
    graph->header->updated_at = time(NULL);
    graph->header->checksum = calculate_checksum(graph->header,
                                                 sizeof(ArachneHeader) - sizeof(uint32_t));
    if (pwrite(graph->fd, graph->header, sizeof(ArachneHeader), 0) != sizeof(ArachneHeader)) {
        perror("pwrite sync");
        pthread_rwlock_unlock(&graph->lock);
        return -1;
    }
    graph->is_dirty = false;
    pthread_rwlock_unlock(&graph->lock);

    return 0;
}

// ============================================================
// AUTO LEARN
// ============================================================
void arachne_set_auto_learn(ArachneGraph* graph, bool enabled) {
    if (graph) graph->auto_learn = enabled;
}

void arachne_set_learning_rate(ArachneGraph* graph, double rate) {
    if (graph && rate > 0 && rate <= 1) graph->learning_rate = rate;
}

// ============================================================
// ADD NODE
// ============================================================
uint64_t arachne_add_node(ArachneGraph* graph, const char* label,
                          NodeState state, double value) {
    if (!graph || !label) {
        fprintf(stderr, "Error: graph or label is NULL\n");
        return 0;
    }
    
    pthread_rwlock_wrlock(&graph->lock);

    if (graph->header->node_count >= MAX_NODES) {
        expand_file(graph, 1024 * 1024 * 10);
    }

    uint64_t id = graph->header->node_count;
    ArachneNode* node = &graph->nodes[id];
    memset(node, 0, sizeof(ArachneNode));

    node->id = id;
    strncpy(node->label, label, MAX_LABEL - 1);
    node->label[MAX_LABEL - 1] = '\0';
    node->state = state;
    node->value = value;
    node->created_at = time(NULL);
    node->updated_at = time(NULL);
    node->start_time = node->created_at;
    node->first_edge = 0;

    graph->header->node_count++;
    graph->is_dirty = true;

    pthread_rwlock_unlock(&graph->lock);
    return id;
}

// ============================================================
// GET NODE
// ============================================================
ArachneNode* arachne_get_node(ArachneGraph* graph, uint64_t id) {
    if (!graph) return NULL;
    if (id >= graph->header->node_count) return NULL;
    return &graph->nodes[id];
}

// ============================================================
// UPDATE NODE
// ============================================================
int arachne_update_node(ArachneGraph* graph, uint64_t id,
                        const char* label, double value) {
    if (!graph) return -1;
    
    pthread_rwlock_wrlock(&graph->lock);

    ArachneNode* node = arachne_get_node(graph, id);
    if (!node) {
        pthread_rwlock_unlock(&graph->lock);
        return -1;
    }

    if (label) {
        strncpy(node->label, label, MAX_LABEL - 1);
        node->label[MAX_LABEL - 1] = '\0';
    }
    node->value = value;
    node->updated_at = time(NULL);
    graph->is_dirty = true;

    pthread_rwlock_unlock(&graph->lock);
    return 0;
}

// ============================================================
// DELETE NODE
// ============================================================
int arachne_delete_node(ArachneGraph* graph, uint64_t id) {
    if (!graph) return -1;
    
    pthread_rwlock_wrlock(&graph->lock);

    ArachneNode* node = arachne_get_node(graph, id);
    if (!node) {
        pthread_rwlock_unlock(&graph->lock);
        return -1;
    }

    uint64_t edge_id = node->first_edge;
    while (edge_id != 0) {
        uint64_t next = graph->edges[edge_id].next_edge;
        memset(&graph->edges[edge_id], 0, sizeof(ArachneEdge));
        graph->header->edge_count--;
        edge_id = next;
    }

    memset(node, 0, sizeof(ArachneNode));
    graph->header->node_count--;
    graph->is_dirty = true;

    pthread_rwlock_unlock(&graph->lock);
    return 0;
}

// ============================================================
// FIND NODE
// ============================================================
uint64_t arachne_find_node(ArachneGraph* graph, const char* label) {
    if (!graph || !label) return 0;
    
    pthread_rwlock_rdlock(&graph->lock);

    for (uint64_t i = 0; i < graph->header->node_count; i++) {
        if (strcmp(graph->nodes[i].label, label) == 0) {
            pthread_rwlock_unlock(&graph->lock);
            return i;
        }
    }

    pthread_rwlock_unlock(&graph->lock);
    return 0;
}

// ============================================================
// SEARCH NODES
// ============================================================
uint64_t* arachne_search_nodes(ArachneGraph* graph, const char* pattern, int* count) {
    if (!graph || !pattern || !count) {
        if (count) *count = 0;
        return NULL;
    }
    
    pthread_rwlock_rdlock(&graph->lock);

    uint64_t* results = malloc(graph->header->node_count * sizeof(uint64_t));
    if (!results) {
        pthread_rwlock_unlock(&graph->lock);
        *count = 0;
        return NULL;
    }
    
    int found = 0;

    for (uint64_t i = 0; i < graph->header->node_count; i++) {
        if (strstr(graph->nodes[i].label, pattern)) {
            results[found++] = i;
        }
    }

    pthread_rwlock_unlock(&graph->lock);
    *count = found;
    return results;
}

// ============================================================
// QUANTUM FUNCTIONS
// ============================================================
int arachne_add_quantum_state(ArachneGraph* graph, uint64_t node_id,
                              double value, double probability) {
    if (!graph) return -1;
    
    pthread_rwlock_wrlock(&graph->lock);

    ArachneNode* node = arachne_get_node(graph, node_id);
    if (!node || node->quantum_count >= MAX_QUANTUM_STATES) {
        pthread_rwlock_unlock(&graph->lock);
        return -1;
    }

    node->quantum_states[node->quantum_count].value = value;
    node->quantum_states[node->quantum_count].probability = probability;
    node->quantum_count++;
    node->state = NODE_SUPERPOSITION;
    graph->is_dirty = true;

    pthread_rwlock_unlock(&graph->lock);
    return 0;
}

int arachne_collapse_node(ArachneGraph* graph, uint64_t node_id) {
    if (!graph) return -1;
    
    pthread_rwlock_wrlock(&graph->lock);

    ArachneNode* node = arachne_get_node(graph, node_id);
    if (!node || node->state != NODE_SUPERPOSITION) {
        pthread_rwlock_unlock(&graph->lock);
        return -1;
    }

    double max_prob = 0;
    double selected_value = node->value;
    for (int i = 0; i < node->quantum_count; i++) {
        if (node->quantum_states[i].probability > max_prob) {
            max_prob = node->quantum_states[i].probability;
            selected_value = node->quantum_states[i].value;
        }
    }

    node->value = selected_value;
    node->state = NODE_ACTUAL;
    node->quantum_count = 0;
    graph->is_dirty = true;

    pthread_rwlock_unlock(&graph->lock);
    return 0;
}

int arachne_entangle_nodes(ArachneGraph* graph, uint64_t node1, uint64_t node2) {
    return arachne_add_edge(graph, node1, node2, "entangled", 1.0);
}

int arachne_disentangle_nodes(ArachneGraph* graph, uint64_t node1, uint64_t node2) {
    if (!graph) return -1;
    
    pthread_rwlock_wrlock(&graph->lock);

    ArachneNode* from = arachne_get_node(graph, node1);
    if (!from) {
        pthread_rwlock_unlock(&graph->lock);
        return -1;
    }

    uint64_t edge_id = from->first_edge;
    uint64_t prev = 0;
    while (edge_id != 0) {
        ArachneEdge* edge = &graph->edges[edge_id];
        if (edge->to_id == node2 && strcmp(edge->relation, "entangled") == 0) {
            if (prev == 0) {
                from->first_edge = edge->next_edge;
            } else {
                graph->edges[prev].next_edge = edge->next_edge;
            }
            memset(edge, 0, sizeof(ArachneEdge));
            graph->header->edge_count--;
            graph->is_dirty = true;
            pthread_rwlock_unlock(&graph->lock);
            return 0;
        }
        prev = edge_id;
        edge_id = edge->next_edge;
    }

    pthread_rwlock_unlock(&graph->lock);
    return -1;
}

// ============================================================
// EDGE FUNCTIONS 
// ============================================================
int arachne_add_edge(ArachneGraph* graph, uint64_t from, uint64_t to,
                     const char* relation, double probability) {
    if (!graph || !relation) {
        fprintf(stderr, "Error: graph or relation is NULL\n");
        return -1;
    }
    
    if (from >= graph->header->node_count || to >= graph->header->node_count) {
        fprintf(stderr, "Error: node %lu or %lu out of range (max: %lu)\n", 
                from, to, graph->header->node_count);
        return -1;
    }
    
    if (probability < 0 || probability > 1) {
        fprintf(stderr, "Error: probability must be between 0 and 1\n");
        return -2;
    }

    pthread_rwlock_wrlock(&graph->lock);

    if (graph->header->edge_count >= MAX_EDGES) {
        fprintf(stderr, "Error: max edges reached\n");
        pthread_rwlock_unlock(&graph->lock);
        return -1;
    }

    uint64_t id = graph->header->edge_count;
    ArachneEdge* edge = &graph->edges[id];
    memset(edge, 0, sizeof(ArachneEdge));

    edge->id = id;
    edge->from_id = from;
    edge->to_id = to;
    strncpy(edge->relation, relation, MAX_RELATION - 1);
    edge->relation[MAX_RELATION - 1] = '\0';
    edge->probability = probability;
    edge->weight = 1.0;
    edge->occurrences = 0;
    edge->successes = 0;
    edge->created_at = time(NULL);
    edge->updated_at = edge->created_at;
    edge->start_time = edge->created_at;
    edge->end_time = 0;

    edge->next_edge = graph->nodes[from].first_edge;
    graph->nodes[from].first_edge = id;

    graph->header->edge_count++;
    graph->is_dirty = true;

    pthread_rwlock_unlock(&graph->lock);
    return 0;
}

int arachne_add_edge_with_context(ArachneGraph* graph, uint64_t from, uint64_t to,
                                  const char* relation, double probability,
                                  const char* context) {
    if (!graph || !relation) {
        fprintf(stderr, "Error: graph or relation is NULL\n");
        return -1;
    }
    
    if (from >= graph->header->node_count || to >= graph->header->node_count) {
        fprintf(stderr, "Error: node %lu or %lu out of range (max: %lu)\n", 
                from, to, graph->header->node_count);
        return -1;
    }
    
    if (probability < 0 || probability > 1) {
        fprintf(stderr, "Error: probability must be between 0 and 1\n");
        return -2;
    }

    pthread_rwlock_wrlock(&graph->lock);

    if (graph->header->edge_count >= MAX_EDGES) {
        fprintf(stderr, "Error: max edges reached\n");
        pthread_rwlock_unlock(&graph->lock);
        return -1;
    }

    uint64_t id = graph->header->edge_count;
    ArachneEdge* edge = &graph->edges[id];
    memset(edge, 0, sizeof(ArachneEdge));

    edge->id = id;
    edge->from_id = from;
    edge->to_id = to;
    strncpy(edge->relation, relation, MAX_RELATION - 1);
    edge->relation[MAX_RELATION - 1] = '\0';
    edge->probability = probability;
    edge->weight = 1.0;
    edge->occurrences = 0;
    edge->successes = 0;
    if (context) {
        strncpy(edge->context, context, MAX_CONTEXT - 1);
        edge->context[MAX_CONTEXT - 1] = '\0';
    }
    edge->created_at = time(NULL);
    edge->updated_at = edge->created_at;
    edge->start_time = edge->created_at;
    edge->end_time = 0;

    edge->next_edge = graph->nodes[from].first_edge;
    graph->nodes[from].first_edge = id;

    graph->header->edge_count++;
    graph->is_dirty = true;

    pthread_rwlock_unlock(&graph->lock);
    return 0;
}

int arachne_add_edge_with_time(ArachneGraph* graph, uint64_t from, uint64_t to,
                               const char* relation, double probability,
                               time_t start_time, time_t end_time) {
    int result = arachne_add_edge(graph, from, to, relation, probability);
    if (result == 0) {
        pthread_rwlock_wrlock(&graph->lock);
        ArachneEdge* edge = &graph->edges[graph->header->edge_count - 1];
        if (edge) {
            edge->start_time = start_time;
            edge->end_time = end_time;
            graph->is_dirty = true;
        }
        pthread_rwlock_unlock(&graph->lock);
    }
    return result;
}

ArachneEdge* arachne_get_edges(ArachneGraph* graph, uint64_t node_id, int* count) {
    if (!graph || !count) {
        if (count) *count = 0;
        return NULL;
    }
    
    if (node_id >= graph->header->node_count) {
        *count = 0;
        return NULL;
    }

    pthread_rwlock_rdlock(&graph->lock);

    int c = 0;
    uint64_t edge_id = graph->nodes[node_id].first_edge;
    while (edge_id != 0) {
        c++;
        edge_id = graph->edges[edge_id].next_edge;
    }

    if (c == 0) {
        pthread_rwlock_unlock(&graph->lock);
        *count = 0;
        return NULL;
    }

    ArachneEdge* result = malloc(c * sizeof(ArachneEdge));
    if (!result) {
        pthread_rwlock_unlock(&graph->lock);
        *count = 0;
        return NULL;
    }

    edge_id = graph->nodes[node_id].first_edge;
    for (int i = 0; i < c && edge_id != 0; i++) {
        result[i] = graph->edges[edge_id];
        edge_id = graph->edges[edge_id].next_edge;
    }

    pthread_rwlock_unlock(&graph->lock);
    *count = c;
    return result;
}
void arachne_debug_edges(ArachneGraph* graph) {
    if (!graph) return;
    
    pthread_rwlock_rdlock(&graph->lock);
    
    printf("\n=== DEBUG: All Edges ===\n");
    printf("Total edges: %lu\n", graph->header->edge_count);
    printf("Total nodes: %lu\n", graph->header->node_count);
    printf("\n");
    

    for (uint64_t i = 0; i < graph->header->node_count; i++) {
        ArachneNode* node = &graph->nodes[i];
        if (node->state == 0) continue;
        
        printf("Node %lu: %s (first_edge=%lu)\n", i, node->label, node->first_edge);
        
        uint64_t edge_id = node->first_edge;
        int count = 0;
        while (edge_id != 0 && edge_id < graph->header->edge_count) {
            ArachneEdge* edge = &graph->edges[edge_id];
            printf("  -> Edge %lu: %lu -> %lu [%s] prob=%.2f next=%lu\n", 
                   edge_id, edge->from_id, edge->to_id, 
                   edge->relation, edge->probability, edge->next_edge);
            edge_id = edge->next_edge;
            count++;
        }
        if (count == 0) {
            printf("  (no edges)\n");
        }
        printf("\n");
    }
    
    printf("=== END DEBUG ===\n\n");
    pthread_rwlock_unlock(&graph->lock);
}
int arachne_update_edge_probability(ArachneGraph* graph, uint64_t from,
                                    uint64_t to, double probability) {
    if (!graph) return -1;
    
    pthread_rwlock_wrlock(&graph->lock);

    ArachneNode* node = arachne_get_node(graph, from);
    if (!node) {
        pthread_rwlock_unlock(&graph->lock);
        return -1;
    }

    uint64_t edge_id = node->first_edge;
    while (edge_id != 0) {
        ArachneEdge* edge = &graph->edges[edge_id];
        if (edge->to_id == to) {
            edge->probability = probability;
            edge->updated_at = time(NULL);
            graph->is_dirty = true;
            pthread_rwlock_unlock(&graph->lock);
            return 0;
        }
        edge_id = edge->next_edge;
    }

    pthread_rwlock_unlock(&graph->lock);
    return -1;
}

int arachne_delete_edge(ArachneGraph* graph, uint64_t id) {
    if (!graph) return -1;
    
    pthread_rwlock_wrlock(&graph->lock);

    if (id >= graph->header->edge_count) {
        pthread_rwlock_unlock(&graph->lock);
        return -1;
    }

    ArachneEdge* edge = &graph->edges[id];
    if (edge->from_id == 0 && edge->to_id == 0) {
        pthread_rwlock_unlock(&graph->lock);
        return -1;
    }

    ArachneNode* from = arachne_get_node(graph, edge->from_id);
    if (from) {
        uint64_t curr = from->first_edge;
        uint64_t prev = 0;
        while (curr != 0) {
            if (curr == id) {
                if (prev == 0) {
                    from->first_edge = edge->next_edge;
                } else {
                    graph->edges[prev].next_edge = edge->next_edge;
                }
                break;
            }
            prev = curr;
            curr = graph->edges[curr].next_edge;
        }
    }

    memset(edge, 0, sizeof(ArachneEdge));
    graph->header->edge_count--;
    graph->is_dirty = true;

    pthread_rwlock_unlock(&graph->lock);
    return 0;
}
