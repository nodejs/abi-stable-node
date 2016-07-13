'use strict';
require('../../common');
var assert = require('assert');

// testing api calls for string
var test_string = require('./build/Release/test_string');

var strings = ['hello world', 'a', '?!@#$%^&*()_+-=\[]{}/.,<>', '\u{2003}\u{2101}\u{2001}'];

strings.forEach(function(element, index) {
  assert.equal(element, test_string.Test(element));
});
