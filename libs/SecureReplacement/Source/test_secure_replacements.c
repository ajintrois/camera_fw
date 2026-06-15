#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "secure_replacement.h"

void print_audit_header() {
    printf("====================================================\n");
    printf("  SECURITY AUDIT TEST SUITE\n");
    printf("  Library: %s\n", libsafec_version());
    printf("====================================================\n\n");
}

// ... (keep previous test functions: test_safe_strlen, test_safe_atoi, etc.)

int main() {
    print_audit_header();
    
    printf("Starting edge-case validation...\n");
    test_safe_strlen();
    test_safe_strncpy();
    test_safe_snprintf();
    test_safe_atoi();
    test_safe_memcpy();
    
    printf("\n=== All Tests Passed Successfully ===\n");
    return 0;
}