import { NgModule }             from '@angular/core';
import { BrowserModule }        from '@angular/platform-browser';
import { FormsModule }          from '@angular/forms';
import { HttpModule }           from '@angular/http';

import { AppComponent }         from './app.component';
import { RoutingModule }        from './routing.module';

import { LoginComponent }       from './login.component';
import { SetupComponent }       from './setup.component';
import { HeaderComponent }      from './header.component';
import { MenuComponent }        from './menu.component';
import { HardwareComponent }    from './hardware.component';
import { DevicesComponent }     from './devices.component';
import { ScreenComponent }      from './screen.component';
import { UsersComponent }       from './users.component';
import { ScriptsComponent }     from './scripts.component';
import { HelpComponent }        from './help.component';
import { TimersComponent }      from './timers.component';

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
	imports:      [ BrowserModule, FormsModule, HttpModule, RoutingModule ],
	declarations: [ AppComponent, LoginComponent, SetupComponent, HeaderComponent, MenuComponent, HardwareComponent, DevicesComponent, ScreenComponent, UsersComponent, ScriptsComponent, HelpComponent, TimersComponent ],
	bootstrap:    [ AppComponent ]
} )

export class AppModule { }
