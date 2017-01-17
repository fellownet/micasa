import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { FormsModule }          from '@angular/forms';
import { GridModule }           from '../grid/grid.module';

import { UsersListComponent }   from './list.component';
import { UserDetailsComponent } from './details.component';
import { LoginComponent }       from './login.component';
//import { UsersService }         from './users.service';
import { UsersRoutingModule }   from './routing.module';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		UsersRoutingModule,
		GridModule
	],
	declarations: [
		UsersListComponent,
		UserDetailsComponent,
		LoginComponent
	]
} )

export class UsersModule {
}
