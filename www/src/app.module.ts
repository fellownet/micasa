import { NgModule }        from '@angular/core';
import { BrowserModule }   from '@angular/platform-browser';
import { FormsModule }     from '@angular/forms';
import { HttpModule }      from '@angular/http';

import { AppComponent }    from './app.component';
import { RoutingModule }   from './routing.module';

import { LoginComponent }  from './login.component';
import { HeaderComponent } from './header.component';
import { MenuComponent }   from './menu.component';
import { ScreenComponent } from './screen.component';
import { UsersComponent }  from './users.component';
import { HelpComponent }   from './help.component';

import { DevicesModule }   from './devices/devices.module';
import { HardwareModule }  from './hardware/hardware.module';
import { ScriptsModule }   from './scripts/scripts.module';
import { TimersModule }    from './timers/timers.module';

import { LoginService }    from './login.service';

// Add the RxJS Observable operators.
// The RxJS library is huge and therefore we only include the operators we need.
import 'rxjs/add/observable/throw';
import 'rxjs/add/operator/catch';
import 'rxjs/add/operator/debounceTime';
import 'rxjs/add/operator/distinctUntilChanged';
import 'rxjs/add/operator/map';
import 'rxjs/add/operator/switchMap';
import 'rxjs/add/operator/toPromise';

@NgModule( {
	imports:      [ BrowserModule, FormsModule, HttpModule, RoutingModule, DevicesModule, HardwareModule, ScriptsModule, TimersModule ],
	declarations: [ AppComponent, LoginComponent, HeaderComponent, MenuComponent, ScreenComponent, UsersComponent, HelpComponent ],
	bootstrap:    [ AppComponent ],
	providers:    [ LoginService ]
} )

export class AppModule { }
