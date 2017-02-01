import { Injectable } from '@angular/core';

export class Option {
	label: string;
	value: string;
}

export class Setting {
	label: string;
	description?: string;
	name: string;
	type: string;
	class: string;
	value?: any;
	minimum?: number;
	maximum?: number;
	maxlength?: number;
	options?: Option[];
}

@Injectable()
export class SettingsService {
}
