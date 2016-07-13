'use strict';
require('../../common');
var assert = require('assert');
var test_number = require('./build/Release/test_number');


// testing api calls for number
var numbers = [1, 100, 2121, 986583];

numbers.forEach(function(element, index) {
  assert.equal(element, test_number.Test(element));
});
