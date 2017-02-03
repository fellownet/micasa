import { NgModule }        from '@angular/core';
import { BrowserModule }   from '@angular/platform-browser';
import { FormsModule }     from '@angular/forms';
import { HttpModule }      from '@angular/http';

import { AppComponent }    from './app.component';
import { RoutingModule }   from './routing.module';

import { DevicesModule }   from './devices/devices.module';
import { HardwareModule }  from './hardware/hardware.module';
import { ScriptsModule }   from './scripts/scripts.module';
import { TimersModule }    from './timers/timers.module';
import { UsersModule }     from './users/users.module';
import { SessionModule }   from './session/session.module';

import { SessionService }  from './session/session.service';

import 'rxjs/add/observable/of';
import 'rxjs/add/observable/throw';
import 'rxjs/add/observable/merge';
import 'rxjs/add/observable/interval';
import 'rxjs/add/operator/mergeMap';
import 'rxjs/add/operator/map';
import 'rxjs/add/operator/every';
import 'rxjs/add/operator/do';
import 'rxjs/add/operator/delay';
import 'rxjs/add/operator/catch';
import 'rxjs/add/operator/filter';
import 'rxjs/add/operator/takeWhile';

@NgModule( {
	imports: [
		BrowserModule,
		FormsModule,
		HttpModule,
		RoutingModule,
		DevicesModule,
		HardwareModule,
		ScriptsModule,
		TimersModule,
		UsersModule,
		SessionModule
	],
	declarations: [
		AppComponent
	],
	bootstrap: [
		AppComponent
	],
	providers: [
		SessionService
	]
} )

export class AppModule {

}
