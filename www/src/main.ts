import { platformBrowserDynamic } from '@angular/platform-browser-dynamic';
import { enableProdMode }         from '@angular/core';
import { AppModule }              from './app.module';

declare var System: any;
if ( typeof( System.import ) == 'undefined' ) {
	enableProdMode();
}

platformBrowserDynamic().bootstrapModule( AppModule );