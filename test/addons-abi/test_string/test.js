'use strict';
var common = require('../../common');
var assert = require('assert');

// testing api calls for string
var test_string = require(`./build/${common.buildType}/test_string`);

var str1 = 'hello world';
assert.equal(test_string.Copy(str1), str1);
assert.equal(test_string.Length(str1), 11);
assert.equal(test_string.Utf8Length(str1), 11);

var str2 = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
assert.equal(test_string.Copy(str2), str2);
assert.equal(test_string.Length(str2), 62);
assert.equal(test_string.Utf8Length(str2), 62);

var str3 = '?!@#$%^&*()_+-=[]{}/.,<>\'\"\\';
assert.equal(test_string.Copy(str3), str3);
assert.equal(test_string.Length(str3), 27);
assert.equal(test_string.Utf8Length(str3), 27);

var str4 = '\u{2003}\u{2101}\u{2001}';
assert.equal(test_string.Copy(str4), str4);
assert.equal(test_string.Length(str4), 3);
assert.equal(test_string.Utf8Length(str4), 9);
