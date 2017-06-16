import {
	Component,
	Input
}                      from '@angular/core';
import {
	NgForm,
	NgModel,
	ValidationErrors } from '@angular/forms';

import { Setting }     from './settings.service';

@Component( {
	selector: 'settings',
	templateUrl: 'tpl/settings.html',
	exportAs: 'settingsComponent'
} )

export class SettingsComponent {

	@Input( "advanced" ) public showAdvancedSettings: boolean = false;

	@Input( "settings" ) public settings: Setting[] = [];
	@Input( "form" ) public form: NgForm;
	@Input( "values" ) public values: any;

	public toggleAdvancedSettings() {
		this.showAdvancedSettings = ! this.showAdvancedSettings;
	};

}
