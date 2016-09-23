'use strict';
require('../../common');
var assert = require('assert');
var test_buffer = require('./build/Release/test_buffer');

var buffer = test_buffer.New("hello world", 12);
var buffer2 = test_buffer.Copy("hello world", 12);

assert.equal(test_buffer.Data(buffer), "hello world");
//assert.equal(test_buffer.Data(buffer2), "hello world");

assert.equal(test_buffer.Length(buffer), 12);
assert.equal(test_buffer.Length(buffer2), 12)
