'use strict';
require('../../common');
var assert = require('assert');

// testing api calls for string
var test_string = require('./build/Release/test_string');


assert(test_string.Test('hello world'), 'hello world');
assert(test_string.Test('ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'), 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789');
assert(test_string.Test('?!@#$%^&*()_+-=\[]{}/.,<>'), '?!@#$%^&*()_+-=\[]{}/.,<>');
assert(test_string.Test('\u{2003}\u{2101}\u{2001}'), '\u{2003}\u{2101}\u{2001}');
