import { NgModule }        from '@angular/core';
import { BrowserModule }   from '@angular/platform-browser';
import { FormsModule }     from '@angular/forms';
import { HttpModule }      from '@angular/http';

import { AppComponent }    from './app.component';
import { RoutingModule }   from './routing.module';

import { HeaderComponent } from './header.component';
import { MenuComponent }   from './menu.component';
import { HelpComponent }   from './help.component';

import { ScreensModule }   from './screens/screens.module';
import { DevicesModule }   from './devices/devices.module';
import { HardwareModule }  from './hardware/hardware.module';
import { ScriptsModule }   from './scripts/scripts.module';
import { TimersModule }    from './timers/timers.module';
import { UsersModule }     from './users/users.module';

import { UsersService }    from './users/users.service';
import { ScreensService }  from './screens/screens.service';

// Add the RxJS Observable operators.
// The RxJS library is huge and therefore we only include the operators we need.
import 'rxjs/add/observable/of';
import 'rxjs/add/observable/throw';
import 'rxjs/add/operator/do';
import 'rxjs/add/operator/delay';
import 'rxjs/add/operator/catch';
import 'rxjs/add/operator/debounceTime';
import 'rxjs/add/operator/distinctUntilChanged';
import 'rxjs/add/operator/map';
import 'rxjs/add/operator/switchMap';
import 'rxjs/add/operator/toPromise';

@NgModule( {
	imports      : [ BrowserModule, FormsModule, HttpModule, RoutingModule, ScreensModule, DevicesModule, HardwareModule, ScriptsModule, TimersModule, UsersModule ],
	declarations : [ AppComponent, HeaderComponent, MenuComponent, HelpComponent ],
	bootstrap    : [ AppComponent ],
	// Creating the UsersService and ScreenService instances here makes sure that there's only one
	// instance of each that all the child modules will inherit (this is the common ancestor).
	providers    : [ UsersService, ScreensService ]
} )

export class AppModule { }
