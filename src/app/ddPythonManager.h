#ifndef __ddPythonManager_h
#define __ddPythonManager_h

#include <ctkAbstractPythonManager.h>

class ddPythonManager : public ctkAbstractPythonManager
{
    Q_OBJECT

public:

  ddPythonManager(QObject* parent=0);
  virtual ~ddPythonManager();

  void setupConsole(QWidget* parent);

public slots:

  void showConsole();

protected:

  virtual void preInitialization();

  virtual QStringList pythonPaths();

  void setupConsoleShortcuts();

  class ddInternal;
  ddInternal* Internal;

  Q_DISABLE_COPY(ddPythonManager);
};

#endif
