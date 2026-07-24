//------------ kuvbur 2022 ------------
#pragma once
#if !defined (ADDON_HPP)
#define	ADDON_HPP

#define	 Menu_OptimizeAnnotations		1

static const Int32 OptimizeAnnotations_CommandID = 1;

void Do_OptimizeAnnotations ();

static GSErrCode MenuCommandHandler (const API_MenuParams* menuParams);
#endif
