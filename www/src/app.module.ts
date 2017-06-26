import { NgModule }       from '@angular/core';
import { BrowserModule }  from '@angular/platform-browser';
import { FormsModule }    from '@angular/forms';
import { HttpModule }     from '@angular/http';

import { AppComponent }   from './app.component';
import { RoutingModule }  from './routing.module';

import { DevicesModule }  from './devices/devices.module';
import { PluginsModule }  from './plugins/plugins.module';
import { ScriptsModule }  from './scripts/scripts.module';
import { TimersModule }   from './timers/timers.module';
import { LinksModule }    from './links/links.module';
import { UsersModule }    from './users/users.module';
import { ScreensModule }  from './screens/screens.module';
import { SessionModule }  from './session/session.module';

import { SessionService } from './session/session.service';
import { ScreensService } from './screens/screens.service';

import 'rxjs/add/observable/of';
import 'rxjs/add/observable/throw';
import 'rxjs/add/observable/forkJoin';
import 'rxjs/add/observable/interval';
import 'rxjs/add/operator/mergeMap';
import 'rxjs/add/operator/map';
import 'rxjs/add/operator/do';
import 'rxjs/add/operator/finally';
import 'rxjs/add/operator/delay';
import 'rxjs/add/operator/debounceTime';
import 'rxjs/add/operator/catch';
import 'rxjs/add/operator/filter';
import 'rxjs/add/operator/takeWhile';
import 'rxjs/add/operator/merge';
import 'rxjs/add/operator/timeout';

import 'jquery/dist/jquery.js'
import 'bootstrap/dist/js/bootstrap.js'

@NgModule( {
	imports: [
		BrowserModule,
		FormsModule,
		HttpModule,
		RoutingModule,
		DevicesModule,
		PluginsModule,
		ScriptsModule,
		TimersModule,
		LinksModule,
		UsersModule,
		ScreensModule,
		SessionModule
	],
	declarations: [
		AppComponent
	],
	bootstrap: [
		AppComponent
	],
	providers: [
		SessionService,
		ScreensService
	]
} )

export class AppModule {

}
