import { NgModule }           from '@angular/core';
import { CommonModule }       from '@angular/common';
import { FormsModule }        from '@angular/forms';
import { GridModule }         from '../grid/grid.module';

import { UsersListComponent } from './list.component';
import { UserEditComponent }  from './edit.component';
import { UsersService }       from './users.service';
import { UsersRoutingModule } from './routing.module';
import { UserResolver }       from './user.resolver';
import { UsersListResolver }  from './list.resolver';

@NgModule( {
	imports: [
		CommonModule,
		FormsModule,
		UsersRoutingModule,
		GridModule
	],
	declarations: [
		UsersListComponent,
		UserEditComponent
	],
	providers: [
		UsersService,
		UserResolver,
		UsersListResolver
	]
} )

export class UsersModule {
}
