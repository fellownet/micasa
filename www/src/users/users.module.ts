import { NgModule }           from '@angular/core';
import { CommonModule }       from '@angular/common';
import { FormsModule }        from '@angular/forms';
import { GridModule }         from '../grid/grid.module';

import { UsersListComponent } from './list.component';
import { UserEditComponent }  from './edit.component';
import { LoginComponent }     from './login.component';
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
		UserEditComponent,
		LoginComponent
	],
	// UsersService is not added as a provider here. Instead it is a provider for the parent app module so
	// all other child modules use the same instance of UserService.
	providers: [
		UserResolver,
		UsersListResolver
	]
} )

export class UsersModule {
}
