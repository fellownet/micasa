// https://angular.io/docs/ts/latest/guide/ngmodule.html#!#bootstrap

import { platformBrowserDynamic } from '@angular/platform-browser-dynamic';
import { enableProdMode }         from '@angular/core';
import { AppModule }              from './app.module';

//enableProdMode();

platformBrowserDynamic().bootstrapModule( AppModule );
