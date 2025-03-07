#pragma once

class EngineApi;

class IScene {
  public:
    virtual ~IScene() {}

    virtual void update(EngineApi &api) = 0;
    virtual void render(EngineApi &api) = 0;

    virtual void on_enter(EngineApi &api);
    virtual void on_exit(EngineApi &api);

    virtual void on_resume(EngineApi &api);
    virtual void on_pause(EngineApi &api);

  private:
};
