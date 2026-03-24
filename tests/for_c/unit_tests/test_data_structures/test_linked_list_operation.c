#include "unity.h"
#include "data_structures/linked_list_operation.h"
#include "data_structures/data_node_operation.h"
#include "utils/memory_manager.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

// Helper to create a key_value_pair
// ===== Helpers =====
static key_value_pair make_kv(const char *key, const char *value) {
    key_value_pair kv;
    kv.key = (char *)key;
    kv.value_size = strlen(value);
    kv.value = (unsigned char *)value;
    return kv;
}

// Helper to create a data_node
static uint64_t random_u64(void) {
	// Use rand() to generate a random 64-bit value
	return ((uint64_t)rand() << 32) | ((uint64_t)rand());
}

static composite_key_hash make_hash(void) {
	composite_key_hash hash;
	hash.bucket_hash = random_u64();
	hash.sub_bucket_hash = random_u64();
	return hash;
}

static data_node *make_data_node(const char *key, const char *value) {
	composite_key_hash hash = make_hash();
	key_value_pair kv = make_kv(key, value);
	data_node *node = NULL;
	int res = create_new_data_node(hash, &kv, false, &node);
	TEST_ASSERT_EQUAL_INT(0, res);
	return node;
}

// ===== Core API Tests =====
void test_create_new_linked_list_node_success(void) {
	data_node *dn = make_data_node("foo", "bar");
	linked_list_node *lln = NULL;
	int res = create_new_linked_list_node(dn->key_hash, dn, &lln);
	TEST_ASSERT_EQUAL_INT(0, res);
	TEST_ASSERT_NOT_NULL(lln);
	TEST_ASSERT_EQUAL_PTR(dn, lln->data_node_ptr);
	free_memory(lln, true);
	delete_data_node(dn);
}

void test_create_new_linked_list_node_null_args(void) {
	linked_list_node *lln = NULL;
	int res = create_new_linked_list_node(make_hash(), NULL, &lln);
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, res);
	data_node *dn = make_data_node("foo", "bar");
	res = create_new_linked_list_node(make_hash(), dn, NULL);
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, res);
	delete_data_node(dn);
}

void test_insert_linked_list_node_success(void) {
	linked_list_node *head = NULL;
	data_node *dn = make_data_node("foo", "bar");
	linked_list_node *lln = NULL;
	create_new_linked_list_node(dn->key_hash, dn, &lln);
	int res = insert_linked_list_node(&head, lln);
	TEST_ASSERT_EQUAL_INT(0, res);
	TEST_ASSERT_EQUAL_PTR(lln, head);
	delete_all_linked_list_nodes(&head);
}

void test_insert_linked_list_node_null_args(void) {
	int res = insert_linked_list_node(NULL, NULL);
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, res);
	linked_list_node *lln = NULL;
	res = insert_linked_list_node(NULL, lln);
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, res);
	linked_list_node *head = NULL;
	res = insert_linked_list_node(&head, NULL);
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, res);
}

void test_get_data_node_from_linked_list_found(void) {
	linked_list_node *head = NULL;
	data_node *dn = make_data_node("foo", "bar");
	linked_list_node *lln = NULL;
	create_new_linked_list_node(dn->key_hash, dn, &lln);
	insert_linked_list_node(&head, lln);
	data_node *out = NULL;
	int res = get_data_node_from_linked_list(head, "foo", dn->key_hash, false, &out);
	TEST_ASSERT_EQUAL_INT(0, res);
	TEST_ASSERT_EQUAL_PTR(dn, out);
	delete_all_linked_list_nodes(&head);
}

void test_get_data_node_from_linked_list_not_found(void) {
	linked_list_node *head = NULL;
	data_node *dn = make_data_node("foo", "bar");
	linked_list_node *lln = NULL;
	create_new_linked_list_node(dn->key_hash, dn, &lln);
	insert_linked_list_node(&head, lln);
	data_node *out = NULL;
	composite_key_hash wrong_hash = { .bucket_hash = 0x1234, .sub_bucket_hash = 0x9999 };
	int res = get_data_node_from_linked_list(head, "baz", make_hash(), false, &out);
	TEST_ASSERT_EQUAL_INT(ERR_DATA_NODE_NOT_FOUND, res);
	res = get_data_node_from_linked_list(head, "foo", wrong_hash, false, &out);
	TEST_ASSERT_EQUAL_INT(ERR_DATA_NODE_NOT_FOUND, res);
	delete_all_linked_list_nodes(&head);
}

void test_get_data_node_from_linked_list_null_args(void) {
	int res = get_data_node_from_linked_list(NULL, NULL, make_hash(), false, NULL);
	TEST_ASSERT_EQUAL_INT(ERR_INVALID_ARGUMENT, res);
}

void test_delete_all_linked_list_nodes_empty(void) {
	linked_list_node *head = NULL;
    int res = delete_all_linked_list_nodes(&head);
    TEST_ASSERT_EQUAL_INT(SUCCESS, res);
}

void test_delete_all_linked_list_nodes_chain(void) {
	linked_list_node *head = NULL;
	data_node *dn1 = make_data_node("foo", "bar");
	data_node *dn2 = make_data_node("baz", "qux");
	linked_list_node *lln1 = NULL, *lln2 = NULL;
	create_new_linked_list_node(dn1->key_hash, dn1, &lln1);
	create_new_linked_list_node(dn2->key_hash, dn2, &lln2);
	insert_linked_list_node(&head, lln1);
	insert_linked_list_node(&head, lln2);
	int res = delete_all_linked_list_nodes(&head);
	TEST_ASSERT_EQUAL_INT(SUCCESS, res);
}

void test_cleanup_deleted_linked_list_nodes_none_deleted(void) {
	linked_list_node *head = NULL;
	data_node *dn = make_data_node("foo", "bar");
	linked_list_node *lln = NULL;
	create_new_linked_list_node(dn->key_hash, dn, &lln);
	insert_linked_list_node(&head, lln);
	int res = cleanup_deleted_linked_list_nodes(&head);
	TEST_ASSERT_EQUAL_INT(0, res);
	delete_all_linked_list_nodes(&head);
}

void test_cleanup_deleted_linked_list_nodes_some_deleted(void) {
	linked_list_node *head = NULL;
	data_node *dn1 = make_data_node("foo", "bar");
	data_node *dn2 = make_data_node("baz", "qux");
	linked_list_node *lln1 = NULL, *lln2 = NULL;
	create_new_linked_list_node(dn1->key_hash, dn1, &lln1);
	create_new_linked_list_node(dn2->key_hash, dn2, &lln2);
	insert_linked_list_node(&head, lln1);
	insert_linked_list_node(&head, lln2);
	// Soft delete dn2 (head)
	dn2->is_deleted = true;
	int res = cleanup_deleted_linked_list_nodes(&head);
	TEST_ASSERT_EQUAL_INT(1, res);
	delete_all_linked_list_nodes(&head);
}

void test_cleanup_deleted_linked_list_nodes_all_deleted(void) {
	linked_list_node *head = NULL;
	data_node *dn1 = make_data_node("foo", "bar");
	linked_list_node *lln1 = NULL;
	create_new_linked_list_node(dn1->key_hash, dn1, &lln1);
	insert_linked_list_node(&head, lln1);
	dn1->is_deleted = true;
	int res = cleanup_deleted_linked_list_nodes(&head);
	TEST_ASSERT_EQUAL_INT(1, res);
	// head should now be NULL
	TEST_ASSERT_NULL(head);
}

void test_cleanup_deleted_linked_list_nodes_null(void) {
	int res = cleanup_deleted_linked_list_nodes(NULL);
	TEST_ASSERT_EQUAL_INT(SUCCESS, res);
}

void test_same_key_different_hash(void) {
    linked_list_node *head = NULL;
    data_node *dn1 = make_data_node("dupkey", "val1");
    data_node *dn2 = make_data_node("dupkey", "val2");
    // Force different hashes
    dn2->key_hash.sub_bucket_hash = dn1->key_hash.sub_bucket_hash + 1;
    linked_list_node *lln1 = NULL, *lln2 = NULL;
    create_new_linked_list_node(dn1->key_hash, dn1, &lln1);
    create_new_linked_list_node(dn2->key_hash, dn2, &lln2);
    insert_linked_list_node(&head, lln1);
    insert_linked_list_node(&head, lln2);
    data_node *out = NULL;
    int res = get_data_node_from_linked_list(head, "dupkey", dn1->key_hash, false, &out);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_PTR(dn1, out);
    res = get_data_node_from_linked_list(head, "dupkey", dn2->key_hash, false, &out);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_PTR(dn2, out);
    delete_all_linked_list_nodes(&head);
}

// Test: Soft-deleted node retrieval
void test_soft_deleted_node_retrieval(void) {
    linked_list_node *head = NULL;
    data_node *dn = make_data_node("softdel", "val");
    linked_list_node *lln = NULL;
    create_new_linked_list_node(dn->key_hash, dn, &lln);
    insert_linked_list_node(&head, lln);
    dn->is_deleted = true;
    data_node *out = NULL;
    int res = get_data_node_from_linked_list(head, "softdel", dn->key_hash, false, &out);
    TEST_ASSERT_EQUAL_INT(ERR_DATA_NODE_NOT_FOUND, res);
    res = get_data_node_from_linked_list(head, "softdel", dn->key_hash, true, &out);
    TEST_ASSERT_EQUAL_INT(0, res);
    TEST_ASSERT_EQUAL_PTR(dn, out);
    delete_all_linked_list_nodes(&head);
}

// Test: Cleanup sets head to NULL
void test_cleanup_sets_head_null(void) {
    linked_list_node *head = NULL;
    data_node *dn = make_data_node("gone", "val");
    linked_list_node *lln = NULL;
    create_new_linked_list_node(dn->key_hash, dn, &lln);
    insert_linked_list_node(&head, lln);
    dn->is_deleted = true;
    int res = cleanup_deleted_linked_list_nodes(&head);
    TEST_ASSERT_EQUAL_INT(1, res);
    TEST_ASSERT_NULL(head);
}

int test_linked_list_operations_main(void) {
	srand((unsigned int)time(NULL)); // Seed random for hash generation
	UNITY_BEGIN();
	RUN_TEST(test_create_new_linked_list_node_success);
	RUN_TEST(test_create_new_linked_list_node_null_args);
	RUN_TEST(test_insert_linked_list_node_success);
	RUN_TEST(test_insert_linked_list_node_null_args);
	RUN_TEST(test_get_data_node_from_linked_list_found);
	RUN_TEST(test_get_data_node_from_linked_list_not_found);
	RUN_TEST(test_get_data_node_from_linked_list_null_args);
	RUN_TEST(test_delete_all_linked_list_nodes_empty);
	RUN_TEST(test_delete_all_linked_list_nodes_chain);
	RUN_TEST(test_cleanup_deleted_linked_list_nodes_none_deleted);
	RUN_TEST(test_cleanup_deleted_linked_list_nodes_some_deleted);
	RUN_TEST(test_cleanup_deleted_linked_list_nodes_all_deleted);
	RUN_TEST(test_cleanup_deleted_linked_list_nodes_null);
    RUN_TEST(test_same_key_different_hash);
    RUN_TEST(test_soft_deleted_node_retrieval);
    RUN_TEST(test_cleanup_sets_head_null);
	return UNITY_END();
}
