import { NgModule }             from '@angular/core';

import { RouterModule, Routes } from '@angular/router';

import { LoginComponent }       from './login.component';
import { SetupComponent }       from './setup.component';
import { HardwareComponent }    from './hardware.component';
import { DevicesComponent }     from './devices.component';
import { ScreenComponent }      from './screen.component';
import { UsersComponent }       from './users.component';
import { ScriptsComponent }     from './scripts.component';
import { HelpComponent }        from './help.component';
import { TimersComponent }      from './timers.component';

// Route Guards
import { LoginService }         from './login.service';
import { ScreenService }        from './screen.service';
import { DeviceService }        from './device.service';

// http://www.gistia.com/working-angular-2-router-part-1/
// https://angular.io/docs/ts/latest/guide/router.html
// http://jasonwatmore.com/post/2016/08/16/angular-2-jwt-authentication-example-tutorial
const routes = [
	{ path: '',            redirectTo: 'dashboard', pathMatch: 'full' },
	{ path: 'login',       component: LoginComponent },
	{ path: 'help',        component: HelpComponent, canActivate: [LoginService] },
	{ path: 'dashboard',   component: ScreenComponent, canActivate: [LoginService], resolve: { screen: ScreenService } },
	{ path: 'setup',       component: SetupComponent, canActivate: [LoginService] },
	{ path: 'hardware',    component: HardwareComponent, canActivate: [LoginService] },
	{ path: 'devices',     component: DevicesComponent, canActivate: [LoginService] },
	{ path: 'devices/:id', component: DevicesComponent, canActivate: [LoginService], resolve: { device: DeviceService } },
	{ path: 'screen/:id',  component: ScreenComponent, canActivate: [LoginService], resolve: { screen: ScreenService } },
	{ path: 'users',       component: UsersComponent, canActivate: [LoginService] },
	{ path: 'scripts',     component: ScriptsComponent, canActivate: [LoginService] },
	{ path: 'timers',      component: TimersComponent, canActivate: [LoginService] },
	{ path: '**',          component: LoginComponent }, // force logout on illegal routes
];

@NgModule( {
	imports:      [ RouterModule.forRoot( routes, { useHash: true } ) ],
	exports:      [ RouterModule ],
	providers:    [ LoginService, ScreenService, DeviceService ]
} )

export class RoutingModule {
}
