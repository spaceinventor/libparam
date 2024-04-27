#include <stdio.h>
#include <string.h>
#include <param/param.h>

#include "param_wildcard.h"

// Function that matches input str with
// given wildcard pattern
// 'n' and 'm' are the length of 'str' and 'pattern' respectively.
int strmatch(const char *str, const char *pattern, int n, int m)  // Source: https://www.geeksforgeeks.org/wildcard-pattern-matching/
{
    // empty pattern can only match with
    // empty string
    if (m == 0)
        return (n == 0);
 
    // lookup table for storing results of
    // subproblems
    int lookup[n + 1][m + 1];
 
    // initialize lookup table to false
    memset(lookup, 0, sizeof(lookup));
 
    // empty pattern can match with empty string
    lookup[0][0] = 1;
 
    // Only '*' can match with empty string
    for (int j = 1; j <= m; j++)
        if (pattern[j - 1] == '*')
            lookup[0][j] = lookup[0][j - 1];
 
    // fill the table in bottom-up fashion
    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= m; j++) {
            // Two cases if we see a '*'
            // a) We ignore ‘*’ character and move
            //    to next  character in the pattern,
            //     i.e., ‘*’ indicates an empty sequence.
            // b) '*' character matches with ith
            //     character in input
            if (pattern[j - 1] == '*')
                lookup[i][j] = lookup[i][j - 1] || lookup[i - 1][j];
 
            // Current characters are considered as
            // matching in two cases
            // (a) current character of pattern is '?'
            // (b) characters actually match
            else if (pattern[j - 1] == '?' || str[i - 1] == pattern[j - 1])
                lookup[i][j] = lookup[i - 1][j - 1];
 
            // If characters don't match
            else
                lookup[i][j] = 0;
        }
    }
 
    return lookup[n][m];
}

int has_wildcard(const char * str, int len) {
    for (int i = 0; i < strlen(str); i++)
		if (str[i] == '*' || str[i] == '?')
			return 1;
    return 0;
}
