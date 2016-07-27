'use strict';
require('../../common');
var assert = require('assert');
var addon = require('./build/Release/binding');

addon(function(msg) {
  assert.equal(msg, 'hello world');
});
