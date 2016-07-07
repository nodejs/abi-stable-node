'use strict';
require('../../common');
var assert = require('assert');
var binding = require('./build/Release/binding');

assert.equal(binding.hello(), 'world');
