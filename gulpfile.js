// Initialize modules
const { src, dest, watch, series, parallel } = require('gulp');
// Importing all the Gulp-related packages we want to use
const sourcemaps = require('gulp-sourcemaps');
const sass = require('gulp-sass');
const concat = require('gulp-concat');
const uglify = require('gulp-uglify-es').default;
const postcss = require('gulp-postcss');
const autoprefixer = require('autoprefixer');
const cssnano = require('cssnano');
const clean = require('gulp-clean');

// File paths
const files = { 
  scssPath: 'client/scss/**/*.scss',
  jsPath: 'client/js/**/*.js'
}

// Sass task: compiles the style.scss file into style.css
function scssTask(){    
  return src(files.scssPath)
      .pipe(sourcemaps.init()) // initialize sourcemaps first
      .pipe(sass()) // compile SCSS to CSS
      .pipe(postcss([ autoprefixer(), cssnano() ])) // PostCSS plugins
      .pipe(sourcemaps.write('.')) // write sourcemaps file in current directory
      .pipe(dest('_dist')
  ); // put final CSS in dist folder
}

// JS task: concatenates and uglifies JS file_s to script.js
function jsTask(){
  return src(files.jsPath)
      .pipe(concat('application.min.js'))
      .pipe(uglify())
      .pipe(dest('_dist')
  );
}

// serves files without concat and minify
function debugTask() {
  return src(files.jsPath)
      .pipe(dest('_dist'));
}

// include external libraries
function includeExternsTask() {
  return src('client/extern/**/*')
      .pipe(dest('_dist/extern'));
}

// clean distribution folder
function cleanTask() {
  return src('_dist', {read: false, allowEmpty: true})
      .pipe(clean());
}

// production build task
exports.build = series(
    cleanTask,
    parallel(scssTask, jsTask, includeExternsTask)
  );

// dev build task
exports.debug = series(
    cleanTask,
    parallel(scssTask, debugTask, includeExternsTask)
);