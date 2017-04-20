( function( global_ ) {

	System.config( {
		paths: {
			// paths serve as alias
			'npm:': 'node_modules/'
		},

		// map tells the System loader where to look for things
		map: {
			// our app is within the app folder
			app: 'app',

			// angular bundles
			'@angular/common'					: 'npm:@angular/common/bundles/common.umd.js',
			'@angular/compiler'					: 'npm:@angular/compiler/bundles/compiler.umd.js',
			'@angular/core'						: 'npm:@angular/core/bundles/core.umd.js',
			'@angular/forms'					: 'npm:@angular/forms/bundles/forms.umd.js',
			'@angular/http'						: 'npm:@angular/http/bundles/http.umd.js',
			'@angular/platform-browser'			: 'npm:@angular/platform-browser/bundles/platform-browser.umd.js',
			'@angular/platform-browser-dynamic'	: 'npm:@angular/platform-browser-dynamic/bundles/platform-browser-dynamic.umd.js',
			'@angular/router'					: 'npm:@angular/router/bundles/router.umd.js',
			'rxjs'								: 'npm:rxjs',
			'ace-builds'						: 'npm:ace-builds',
			'jquery'							: 'npm:jquery',
			'bootstrap'							: 'npm:bootstrap',
			'highcharts'						: 'npm:highcharts'
		},

		// packages tells the System loader how to load when no filename and/or no extension
		packages: {
			app: {
				main: './main.js',
				defaultExtension: 'js'
			},
			rxjs: {
				defaultExtension: 'js'
			}
		}
	} );

}( this ) );
